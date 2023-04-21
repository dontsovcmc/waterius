#ifndef _STORAGE_h
#define _STORAGE_h

#include <Arduino.h>
#include <EEPROM.h>

#include "Setup.h"

uint8_t crc_8(unsigned char *input_str, size_t num_bytes);

template <class T>
class EEPROMStorage
{
    /*
    Кольцевой буфер в памяти для хранения 1го элемента класса Т. Чтобы превысить максимальное
    число записи в EEPROM (100 000).

    blocks - кол-во копий Т (размер кольцевого буфера).

    После блоков следует массив контрольных сумм crc8 блоков буфера.

    При перезагрузке микроконтроллера блок с максимальным значением счетчиков восстановится и буфер продолжит работу.

    |xxxx|xxxx|xxxx|xxxx|crc|crc|crc|crc|
    */

public:
    explicit EEPROMStorage(const uint8_t _blocks, const uint8_t _start_addr = 0);
    void add(const T &element);
    bool get(T &element);
    bool get_block(const uint8_t block, T &element);
    void clear();

    uint16_t size();

private:
    uint8_t start_addr;
    uint8_t activeBlock;
    uint8_t blocks;

    uint8_t elementSize;

    uint16_t flag_shift;
    int8_t compare(const T &element1, const T &element2);
};

#endif
