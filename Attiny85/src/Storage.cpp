#include "Storage.h"

// https://gist.github.com/brimston3/83cdeda8f7d2cf55717b83f0d32f9b5e
// https://www.onlinegdb.com/online_c++_compiler
// Dallas CRC x8+x5+x4+1
uint8_t crc_8(unsigned char *b, size_t num_bytes)
{
    uint8_t i, crc = 0;
    for (size_t a = 0; a < num_bytes; a++)
    {
        i = (*(b + a) ^ crc) & 0xff;
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
    }
    return crc;
}

template <class T>
EEPROMStorage<T>::EEPROMStorage(const uint8_t _blocks, const uint8_t _start_addr)
    : start_addr(_start_addr), activeBlock(0), blocks(_blocks)
{
    elementSize = sizeof(T);
    flag_shift = start_addr + elementSize * blocks;

    //тут определяем не испорчена ли память
    int t = 0;
    T tmp;
    for (int i = 0; i < blocks; i++)
    {
        t += EEPROM.read(flag_shift + i);
        if (t > 0)
        {
            activeBlock = i;
            if (get(tmp))
                return;
        }
    }

    activeBlock = 0;
    for (uint16_t i = start_addr; i < flag_shift + blocks; i++)
    {
        EEPROM.write(i, 0);
    }
}

template <class T>
void EEPROMStorage<T>::add(const T &element)
{
    uint8_t prev = activeBlock;
    activeBlock = (activeBlock < blocks - 1) ? activeBlock + 1 : 0;

    EEPROM.put(start_addr + activeBlock * elementSize, element);
    uint8_t mark = crc_8((unsigned char *)&element, elementSize);
    if (mark == 0)
        mark++;
    EEPROM.write(flag_shift + activeBlock, mark);
    EEPROM.write(flag_shift + prev, 0);
}

template <class T>
bool EEPROMStorage<T>::get(T &element)
{
    return get_block(activeBlock, element);
}

template <class T>
bool EEPROMStorage<T>::get_block(const uint8_t block, T &element)
{
    T tmp;
    EEPROM.get(start_addr + block * elementSize, tmp);

    uint8_t crc = crc_8((unsigned char *)&tmp, elementSize);
    uint8_t mark = EEPROM.read(flag_shift + block);
    if (mark == crc || (mark == 1 && crc == 0))
    {
        element = tmp;
        return true;
    }
    return false;
}

template <class T>
uint16_t EEPROMStorage<T>::size()
{
    return flag_shift + blocks;
}

template class EEPROMStorage<Data>;
