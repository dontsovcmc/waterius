#include "string.h"

inline void strncpy0(char *dest, const char *src, const size_t len)
{   
    strncpy(dest, src, len-1);
    dest[len-1] = '\0';
}
		