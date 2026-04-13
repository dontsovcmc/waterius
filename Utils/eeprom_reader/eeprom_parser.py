#!/usr/bin/env python3
"""
Waterius Attiny85 EEPROM Dump Reader

Читает дамп EEPROM Attiny85 (Intel HEX или raw binary) и выводит
содержимое: показания счётчиков (кольцевой буфер) и конфигурацию.

Чтение дампа с устройства (USBasp программатор):
  avrdude -p t85 -c usbasp -B 4 -P usb -U eeprom:r:eeprom_dump.hex:i

Запуск скрипта:
  python3 eeprom_parser.py eeprom_dump.hex              # Intel HEX от avrdude
  python3 eeprom_parser.py eeprom_dump.bin              # raw binary
  python3 eeprom_parser.py eeprom_dump.hex --hex-dump   # + полный hex дамп

Поддерживаемые версии прошивки attiny85:
  v12-v26 (старый формат): CRC init=0, один флаг на активный блок,
           предыдущий флаг обнуляется. Config не хранится в EEPROM.
  v27+    (новый формат):  CRC init=0xFF, все блоки хранят CRC,
           активный = блок с максимальными показаниями.
           Config ring buffer (модель, типы входов, перезагрузки).

Формат определяется автоматически по CRC-области.

EEPROM layout (512 bytes, Attiny85):

  Offset    Размер  Описание
  --------  ------  ------------------------------------------
  0-159     160     Data ring buffer: 20 блоков по 8 байт
                      value0: uint32_t - импульсы вход 0 (LE)
                      value1: uint32_t - импульсы вход 1 (LE)
  160-179   20      CRC-8 / флаг для каждого Data блока
  180-189   10      Config ring buffer: 2 блока по 5 байт (v27+)
                      setup_started_counter: uint8_t
                      resets: uint8_t
                      model: uint8_t (0=Classic, 2=Waterius-2)
                      type0: uint8_t (тип входа 0)
                      type1: uint8_t (тип входа 1)
  190-191   2       CRC-8 для каждого Config блока (v27+)
  192-511   320     Не используется

  В старых прошивках (v12-v26) по offset 180 хранился resets,
  по offset 181 — setup_started_counter (вне ring buffer).

Типы входов (CounterType):
  0 = NAMUR, 1 = DISCRETE, 2 = ELECTRONIC, 3 = HALL, 255 = NONE

Исходники Attiny85:
  Attiny85/src/Setup.h     — структуры Data, Config, Header
  Attiny85/src/Storage.cpp — CRC-8, кольцевой буфер, алгоритм compare
  Attiny85/src/Storage.h   — шаблон EEPROMStorage<T>
"""

import sys
import struct
import argparse

# --- Constants ---

DATA_BLOCKS = 20
DATA_SIZE = 8       # sizeof(Data): uint32_t value0 + uint32_t value1
DATA_START = 0
DATA_CRC_START = DATA_START + DATA_SIZE * DATA_BLOCKS  # 160

CONFIG_BLOCKS = 2
CONFIG_SIZE = 5     # sizeof(Config)
CONFIG_START = DATA_CRC_START + DATA_BLOCKS             # 180
CONFIG_CRC_START = CONFIG_START + CONFIG_SIZE * CONFIG_BLOCKS  # 190

TOTAL_USED = CONFIG_CRC_START + CONFIG_BLOCKS            # 192

COUNTER_TYPE = {
    0: "NAMUR",
    1: "DISCRETE",
    2: "ELECTRONIC",
    3: "HALL",
    255: "NONE",
}

MODEL_NAME = {
    0: "Classic (2 канала)",
    2: "Waterius-2",
}


# --- CRC-8 (Dallas CRC x^8+x^5+x^4+1) ---
# Порт из Attiny85/src/Storage.cpp

def crc_8_byte(b, crc):
    i = (b ^ crc) & 0xFF
    crc = 0
    if i & 1:
        crc ^= 0x5E
    if i & 2:
        crc ^= 0xBC
    if i & 4:
        crc ^= 0x61
    if i & 8:
        crc ^= 0xC2
    if i & 0x10:
        crc ^= 0x9D
    if i & 0x20:
        crc ^= 0x23
    if i & 0x40:
        crc ^= 0x46
    if i & 0x80:
        crc ^= 0x8C
    return crc


def crc_8(data, init=0xFF):
    crc = init
    for b in data:
        crc = crc_8_byte(b, crc)
    return crc


# --- Intel HEX parser ---

def parse_intel_hex(text):
    """Парсит Intel HEX формат, возвращает bytearray."""
    result = {}
    for line_num, line in enumerate(text.splitlines(), 1):
        line = line.strip()
        if not line:
            continue
        if not line.startswith(':'):
            raise ValueError(f"Строка {line_num}: ожидается ':', получено '{line[:10]}'")

        raw = bytes.fromhex(line[1:])
        if len(raw) < 5:
            raise ValueError(f"Строка {line_num}: слишком короткая")

        byte_count = raw[0]
        address = (raw[1] << 8) | raw[2]
        rec_type = raw[3]
        data = raw[4:-1]
        checksum = raw[-1]

        # Проверка контрольной суммы строки
        if sum(raw) & 0xFF != 0:
            raise ValueError(f"Строка {line_num}: неверная контрольная сумма")

        if rec_type == 0x01:  # EOF
            break
        if rec_type == 0x00:  # Data record
            if len(data) != byte_count:
                raise ValueError(f"Строка {line_num}: длина данных не совпадает")
            for offset, byte in enumerate(data):
                result[address + offset] = byte

    if not result:
        raise ValueError("Нет данных в HEX файле")

    min_addr = min(result.keys())
    max_addr = max(result.keys())
    buf = bytearray(max_addr - min_addr + 1)
    for addr, byte in result.items():
        buf[addr - min_addr] = byte
    return buf


def load_file(path):
    """Загружает файл, автоматически определяя формат (Intel HEX / raw binary)."""
    with open(path, 'rb') as f:
        raw = f.read()

    # Пробуем Intel HEX
    try:
        text = raw.decode('ascii', errors='strict')
        if text.lstrip().startswith(':'):
            return parse_intel_hex(text)
    except (UnicodeDecodeError, ValueError):
        pass

    return bytearray(raw)


# --- Ring buffer compare (порт из Storage.cpp) ---
# Сравнивает два блока побайтово с конца (little-endian comparison)

def compare_blocks(data, block1_data, block2_data):
    """Сравнивает два Data блока как в Storage.cpp::compare().
    Возвращает 1 если block1 > block2, -1 если <, 0 если равны."""
    for i in range(len(block1_data) - 1, -1, -1):
        if block1_data[i] > block2_data[i]:
            return 1
        elif block1_data[i] < block2_data[i]:
            return -1
    return 0


# --- Format detection ---

def detect_storage_format(eeprom):
    """Определяет формат хранения по CRC-области Data блоков.

    Старый формат (v21-26): только 1 ненулевой флаг, CRC init=0.
    Новый формат (v27+):    несколько ненулевых CRC, init=0xFF.

    Возвращает: 'old' или 'new'
    """
    flags = eeprom[DATA_CRC_START:DATA_CRC_START + DATA_BLOCKS]
    nonzero = sum(1 for f in flags if f != 0)

    if nonzero <= 1:
        # Проверяем: если единственный ненулевой флаг совпадает с CRC(init=0)
        for i, flag in enumerate(flags):
            if flag != 0:
                offset = DATA_START + i * DATA_SIZE
                block_bytes = eeprom[offset:offset + DATA_SIZE]
                crc_old = crc_8(block_bytes, init=0)
                if flag == crc_old or (flag == 1 and crc_old == 0):
                    return 'old'
        return 'old'  # Все нули — пустой EEPROM, неважно какой формат

    return 'new'


# --- Parsing ---

def parse_data_blocks(eeprom, fmt):
    """Парсит Data ring buffer. Возвращает список блоков и индекс активного."""
    crc_init = 0x00 if fmt == 'old' else 0xFF
    blocks = []

    for i in range(DATA_BLOCKS):
        offset = DATA_START + i * DATA_SIZE
        block_bytes = eeprom[offset:offset + DATA_SIZE]

        if len(block_bytes) < DATA_SIZE:
            blocks.append({"index": i, "valid": False, "value0": 0, "value1": 0,
                           "raw": block_bytes, "crc_stored": None, "crc_calc": None})
            continue

        value0, value1 = struct.unpack_from('<II', block_bytes)
        crc_calc = crc_8(block_bytes, init=crc_init)

        crc_offset = DATA_CRC_START + i
        crc_stored = eeprom[crc_offset] if crc_offset < len(eeprom) else None

        if crc_stored is not None:
            if fmt == 'old':
                # Старый формат: флаг==CRC или (флаг==1 и CRC==0)
                valid = (crc_stored == crc_calc) or (crc_stored == 1 and crc_calc == 0)
            else:
                valid = crc_stored == crc_calc
        else:
            valid = False

        blocks.append({
            "index": i,
            "valid": valid,
            "value0": value0,
            "value1": value1,
            "raw": block_bytes,
            "crc_stored": crc_stored,
            "crc_calc": crc_calc,
        })

    if fmt == 'old':
        # Старый формат: активный — единственный блок с ненулевым флагом
        active = -1
        for i, blk in enumerate(blocks):
            if blk["crc_stored"] and blk["crc_stored"] != 0:
                active = i
                break
    else:
        # Новый формат: активный — максимальный среди валидных
        active = -1
        for i, blk in enumerate(blocks):
            if blk["valid"]:
                if active < 0:
                    active = i
                elif compare_blocks(eeprom, blk["raw"], blocks[active]["raw"]) > 0:
                    active = i

    return blocks, active


def parse_config_blocks(eeprom, fmt):
    """Парсит Config ring buffer. Возвращает список блоков и индекс активного."""
    if fmt == 'old':
        return [], -1  # Config не существует в старом формате

    if len(eeprom) < CONFIG_CRC_START + CONFIG_BLOCKS:
        return [], -1

    blocks = []
    for i in range(CONFIG_BLOCKS):
        offset = CONFIG_START + i * CONFIG_SIZE
        block_bytes = eeprom[offset:offset + CONFIG_SIZE]

        if len(block_bytes) < CONFIG_SIZE:
            blocks.append({"index": i, "valid": False, "raw": block_bytes})
            continue

        crc_calc = crc_8(block_bytes)
        crc_offset = CONFIG_CRC_START + i
        crc_stored = eeprom[crc_offset] if crc_offset < len(eeprom) else None
        valid = crc_stored == crc_calc if crc_stored is not None else False

        setup_started, resets, model, type0, type1 = struct.unpack_from('<BBBBB', block_bytes)

        blocks.append({
            "index": i,
            "valid": valid,
            "setup_started_counter": setup_started,
            "resets": resets,
            "model": model,
            "type0": type0,
            "type1": type1,
            "raw": block_bytes,
            "crc_stored": crc_stored,
            "crc_calc": crc_calc,
        })

    # Активный — с максимальными значениями среди валидных
    active = -1
    for i, blk in enumerate(blocks):
        if blk["valid"]:
            if active < 0:
                active = i
            elif compare_blocks(eeprom, blk["raw"], blocks[active]["raw"]) > 0:
                active = i

    return blocks, active


# --- Display ---

def fmt_counter_type(t):
    return COUNTER_TYPE.get(t, f"UNKNOWN({t})")


def fmt_model(m):
    return MODEL_NAME.get(m, f"UNKNOWN({m})")


def print_header(title):
    print(f"\n{'=' * 60}")
    print(f"  {title}")
    print(f"{'=' * 60}")


def print_summary(data_blocks, data_active, config_blocks, config_active, fmt):
    """Выводит сводку."""
    print_header("СВОДКА")

    fmt_name = "старый (v21-26, CRC init=0)" if fmt == 'old' else "новый (v27+, CRC init=0xFF)"
    print(f"  Формат хранения:   {fmt_name}")

    if data_active >= 0:
        blk = data_blocks[data_active]
        print(f"  Показания вход 0 (value0): {blk['value0']}")
        print(f"  Показания вход 1 (value1): {blk['value1']}")
        print(f"  Активный Data блок: #{data_active}")
    else:
        print("  Данные: НЕ НАЙДЕНЫ (нет валидных Data блоков)")

    if fmt == 'old':
        flagged = sum(1 for b in data_blocks if b["crc_stored"] and b["crc_stored"] != 0)
        print(f"  Блоков с флагом: {flagged}/{DATA_BLOCKS}")
    else:
        valid_count = sum(1 for b in data_blocks if b["valid"])
        print(f"  Валидных Data блоков: {valid_count}/{DATA_BLOCKS}")

    if config_blocks:
        if config_active >= 0:
            cfg = config_blocks[config_active]
            print()
            print(f"  Модель:             {fmt_model(cfg['model'])} ({cfg['model']})")
            print(f"  Тип входа 0:        {fmt_counter_type(cfg['type0'])}")
            print(f"  Тип входа 1:        {fmt_counter_type(cfg['type1'])}")
            print(f"  Перезагрузок:       {cfg['resets']}")
            print(f"  Настроек:           {cfg['setup_started_counter']}")
            print(f"  Активный Config блок: #{config_active}")
        else:
            print("\n  Config: НЕ НАЙДЕН (нет валидных Config блоков)")
            print("  (прошивка attiny < 27 или Config не записан)")
    else:
        print("\n  Config: НЕТ (файл слишком мал или прошивка < v27)")


def print_data_table(data_blocks, data_active, fmt):
    """Выводит таблицу всех Data блоков."""
    print_header("DATA RING BUFFER (20 блоков × 8 байт)")
    flag_col = "Flag" if fmt == 'old' else "CRC"
    print(f"  {'#':>2}  {'value0':>10}  {'value1':>10}  {flag_col:>5}  {'Статус'}")
    print(f"  {'--':>2}  {'------':>10}  {'------':>10}  {'---':>5}  {'------'}")

    for blk in data_blocks:
        i = blk["index"]
        marker = " <-- ACTIVE" if i == data_active else ""

        if fmt == 'old':
            flag = blk["crc_stored"]
            if flag and flag != 0:
                if blk["valid"]:
                    status = f"OK (flag=CRC){marker}"
                else:
                    status = f"BAD (flag=0x{flag:02X}, CRC=0x{blk['crc_calc']:02X}){marker}"
            else:
                status = f"нет флага{marker}"
        else:
            if blk["valid"]:
                status = f"OK{marker}"
            elif blk["crc_stored"] is not None:
                status = f"BAD CRC (stored=0x{blk['crc_stored']:02X}, calc=0x{blk['crc_calc']:02X}){marker}"
            else:
                status = "N/A"

        crc_str = f"0x{blk['crc_stored']:02X}" if blk['crc_stored'] is not None else "N/A"
        print(f"  {i:>2}  {blk['value0']:>10}  {blk['value1']:>10}  {crc_str:>5}  {status}")


def print_config_table(config_blocks, config_active):
    """Выводит таблицу Config блоков."""
    if not config_blocks:
        return

    print_header("CONFIG RING BUFFER (2 блока × 5 байт)")
    for blk in config_blocks:
        i = blk["index"]
        marker = " <-- ACTIVE" if i == config_active else ""

        if blk["valid"]:
            print(f"  Блок #{i}: OK{marker}")
            print(f"    setup_started_counter = {blk['setup_started_counter']}")
            print(f"    resets                = {blk['resets']}")
            print(f"    model                 = {fmt_model(blk['model'])} ({blk['model']})")
            print(f"    type0                 = {fmt_counter_type(blk['type0'])}")
            print(f"    type1                 = {fmt_counter_type(blk['type1'])}")
            print(f"    CRC: stored=0x{blk['crc_stored']:02X}, calc=0x{blk['crc_calc']:02X}")
        else:
            crc_s = f"0x{blk['crc_stored']:02X}" if blk.get('crc_stored') is not None else "N/A"
            crc_c = f"0x{blk['crc_calc']:02X}" if blk.get('crc_calc') is not None else "N/A"
            print(f"  Блок #{i}: BAD CRC (stored={crc_s}, calc={crc_c}){marker}")


def print_raw_dump(eeprom):
    """Выводит hex дамп EEPROM."""
    print_header("HEX ДАМП")
    total = len(eeprom)
    print(f"  Размер файла: {total} байт")

    for offset in range(0, total, 16):
        chunk = eeprom[offset:offset + 16]
        hex_part = ' '.join(f'{b:02X}' for b in chunk)
        ascii_part = ''.join(chr(b) if 32 <= b < 127 else '.' for b in chunk)

        # Пометки областей
        label = ""
        if offset < DATA_CRC_START:
            block_num = offset // DATA_SIZE
            label = f"  Data[{block_num}]"
        elif offset < CONFIG_START:
            label = "  Data CRC"
        elif offset < CONFIG_CRC_START:
            block_num = (offset - CONFIG_START) // CONFIG_SIZE
            label = f"  Config[{block_num}]"
        elif offset < TOTAL_USED:
            label = "  Config CRC"

        print(f"  {offset:04X}: {hex_part:<48s} |{ascii_part}|{label}")


def main():
    parser = argparse.ArgumentParser(
        description="Waterius Attiny85 EEPROM Dump Reader",
        epilog="Поддерживает Intel HEX (.hex) и raw binary форматы.",
    )
    parser.add_argument("file", help="Путь к файлу дампа EEPROM")
    parser.add_argument("--hex-dump", action="store_true",
                        help="Показать полный hex дамп")
    args = parser.parse_args()

    try:
        eeprom = load_file(args.file)
    except FileNotFoundError:
        print(f"Ошибка: файл '{args.file}' не найден", file=sys.stderr)
        sys.exit(1)
    except ValueError as e:
        print(f"Ошибка чтения файла: {e}", file=sys.stderr)
        sys.exit(1)

    print(f"Загружен файл: {args.file} ({len(eeprom)} байт)")

    if len(eeprom) < DATA_CRC_START + DATA_BLOCKS:
        print(f"Ошибка: файл слишком мал (минимум {DATA_CRC_START + DATA_BLOCKS} байт)", file=sys.stderr)
        sys.exit(1)

    fmt = detect_storage_format(eeprom)
    data_blocks, data_active = parse_data_blocks(eeprom, fmt)
    config_blocks, config_active = parse_config_blocks(eeprom, fmt)

    print_summary(data_blocks, data_active, config_blocks, config_active, fmt)
    print_data_table(data_blocks, data_active, fmt)
    print_config_table(config_blocks, config_active)

    if args.hex_dump:
        print_raw_dump(eeprom)


if __name__ == '__main__':
    main()
