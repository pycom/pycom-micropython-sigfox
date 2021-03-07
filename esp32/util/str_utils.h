#ifndef STR_UTILS_H
#define STR_UTILS_H

#include <stddef.h>
#include <stdint.h>

/**
 * Determine the length of str at compile time
 *
 * The length is excluding the terminating \0 just as strlen()
 * make sure you pass a compile time constant
 */
#define strlen_const(string) (sizeof(string)-1)

void sprint_binary_u8(char* s, uint8_t v);

void hexdump(const uint8_t* buf, size_t len);

#endif
