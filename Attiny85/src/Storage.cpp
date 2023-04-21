#include "Storage.h"

// https://gist.github.com/brimston3/83cdeda8f7d2cf55717b83f0d32f9b5e
// https://www.onlinegdb.com/online_c++_compiler
// Dallas CRC x8+x5+x4+1
uint8_t crc_8(unsigned char *b, size_t num_bytes)
{
    uint8_t i, crc = 0xff;
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

    // Поиск последней записи
    int n = -1, i = 0;
    T tmp, max;
    do {
        if (get_block(i, tmp))
        {
            // Блок успешно прочитан
            if (n < 0)
            {
                // И это первый успешно прочитанный блок
                n = i;
                max = tmp;
            }
            else
            {
                if (compare(tmp, max) > 0)
                {
                    // Прочитанный блок содержит большее значение счетчиков
                    n = i;
                    max = tmp;
                }
            }
        }
        // Читаем все блоки
        i++;
    } while (i < blocks);

    if (n >= 0)
    {
        // Блок с максимальным значением счетчиков нашли
        activeBlock = n;
        return;
    }

    // Ни одного блока прочитать не удалось
    clear();
}

template <class T>
void EEPROMStorage<T>::clear()
{
    activeBlock = 0;
    for (uint16_t i = start_addr; i < flag_shift + blocks; i++)
    {
        EEPROM.write(i, 0);
    }
}

// Функция сравнения элементов, что бы не добавлять дополнительных полей используем информацию о том
// что элемент состоит из увеличивающихся счетчиков, а так как AVR little-endian, то сравниваем с конца
template <class T>
int8_t EEPROMStorage<T>::compare(const T &element1, const T &element2)
{
    unsigned char *e1 = (unsigned char *)&element1 + elementSize;
    unsigned char *e2 = (unsigned char *)&element2 + elementSize;
    while (e1 >= (unsigned char *)&element1)
    {
        if (*e1 > *e2)
            return 1;
        else if (*e1 < *e2)
            return -1;
        e1--;
        e2--;    
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
bool EEPROMStorage<T>::get(T &element)
{
    T tmp;
    if (get_block(activeBlock, tmp))
    {
        element = tmp;
        return true;
    }
    return false;
}

template <class T>
bool EEPROMStorage<T>::get_block(const uint8_t block, T &element)
{
    EEPROM.get(start_addr + block * elementSize, element);

    uint8_t crc = crc_8((unsigned char *)&element, elementSize);
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
