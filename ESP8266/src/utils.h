#ifndef _WATERIUS_INIT_h
#define _WATERIUS_INIT_h

#include "c_types.h"
#include "string.h"

/*
Запишем 0 в конце буфера принудительно.
*/

inline void strncpy0(char *dest, const char *src, const size_t len)
{   
    strncpy(dest, src, len-1);
    dest[len-1] = '\0';
} 

unsigned int CRC16(unsigned int crc, char *buf, const int len);

bool setClock();

#endif