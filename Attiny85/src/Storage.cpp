#include <avr/wdt.h>
#include "Storage.h"
#include <EEPROM.h>

// https://gist.github.com/brimston3/83cdeda8f7d2cf55717b83f0d32f9b5e
// https://www.onlinegdb.com/online_c++_compiler
// Dallas CRC x8+x5+x4+1
uint8_t crc_8_byte(unsigned char b, uint8_t crc)
{
    uint8_t i = (b ^ crc) & 0xff;
    crc = 0;
    if (i & 1)
        crc ^= 0x5e;
    if (i & 2)
        crc ^= 0xbc;
    if (i & 4)
        crc ^= 0x61;
    if (i & 8)
        crc ^= 0xc2;
    if (i & 0x10)
        crc ^= 0x9d;
    if (i & 0x20)
        crc ^= 0x23;
    if (i & 0x40)
        crc ^= 0x46;
    if (i & 0x80)
        crc ^= 0x8c;
    return crc;
}

uint8_t crc_8(unsigned char *b, size_t num_bytes)
{
    uint8_t crc = 0xff;
    for (size_t a = 0; a < num_bytes; a++)
    {
        crc = crc_8_byte(*(b + a), crc);
    }
    return crc;
}

template <class T>
EEPROMStorage<T>::EEPROMStorage(const uint8_t _blocks, const uint8_t _start_addr)
    : start_addr(_start_addr), activeBlock(0), blocks(_blocks)
{
    elementSize = sizeof(T);
    flag_shift = start_addr + elementSize * blocks;
}

template <class T>
bool EEPROMStorage<T>::init()
{
    // Поиск последней записи
    int n = -1, i = 0;
    do {
        if (check_block(i))
        {
            // Блок успешно прочитан
            if (n < 0)
            {
                // И это первый успешно прочитанный блок
                n = i;
            }
            else
            {
                if (compare(i, n) > 0)
                {
                    // Прочитанный блок содержит большее значение счетчиков
                    n = i;
                }
            }
        }

        // Читаем все блоки
        i++;
        wdt_reset();
    } while (i < blocks);

    if (n >= 0)
    {
        // Блок с максимальным значением счетчиков нашли
        activeBlock = n;
        return true;
    }

    // Ни одного блока прочитать не удалось
    activeBlock = 0;
    return false;
}

template <class T>
void EEPROMStorage<T>::clear()
{
    activeBlock = 0;
    for (uint16_t i = start_addr; i < flag_shift + blocks; i++)
    {
        EEPROM.write(i, 0);
        wdt_reset();
    }
}

// Функция сравнения элементов, что бы не добавлять дополнительных полей используем информацию о том
// что элемент состоит из увеличивающихся счетчиков, а так как AVR little-endian, то сравниваем с конца
template <class T>
int8_t EEPROMStorage<T>::compare(const uint8_t block1, const uint8_t block2)
{
    uint8_t length = elementSize;
    uint8_t addr1 = start_addr + block1 * length + length - 1;
    uint8_t addr2 = start_addr + block2 * length + length - 1;
    while (length--)
    {
        uint8_t d1 = EEPROM.read(addr1);
        uint8_t d2 = EEPROM.read(addr2);
        if (d1 > d2)
            return 1;
        else if (d1 < d2)
            return -1;
        addr1--;
        addr2--;    
    }
    return 0;
}

template <class T>
void EEPROMStorage<T>::add(const T &element)
{
    activeBlock = (activeBlock < blocks - 1) ? activeBlock + 1 : 0;

    EEPROM.put(start_addr + activeBlock * elementSize, element);
    uint8_t mark = crc_8((unsigned char *)&element, elementSize);
    EEPROM.write(flag_shift + activeBlock, mark);
}

template <class T>
bool EEPROMStorage<T>::get(T &element, uint8_t block)
{
    if (block == (uint8_t)-1)
    {
        block = activeBlock;
    }

    EEPROM.get(start_addr + block * elementSize, element);

    uint8_t crc = crc_8((unsigned char *)&element, elementSize);
    uint8_t mark = EEPROM.read(flag_shift + block);
    return (mark == crc);
}

template <class T>
bool EEPROMStorage<T>::check_block(const uint8_t block)
{
    uint8_t addr = start_addr + block * elementSize;
    uint8_t length = elementSize;
    uint8_t crc = 0xff;
    while (length--)
    {
        crc = crc_8_byte(EEPROM.read(addr), crc);
        addr++;
    }
    uint8_t mark = EEPROM.read(flag_shift + block);
    return (mark == crc);
}

template <class T>
uint16_t EEPROMStorage<T>::size()
{
    return flag_shift + blocks;
}

template class EEPROMStorage<Data>;
template class EEPROMStorage<Config>;
