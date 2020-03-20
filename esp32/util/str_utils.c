#include "str_utils.h"
#include <stdio.h>

/**
 * Create a string representation of a uint8
 */
void sprint_binary_u8(char* s, uint8_t v){
  size_t len = 9; // eight bits plus '\0'
  snprintf(s, len, "%u%u%u%u%u%u%u%u",
           (v & 0b10000000) >> 7,
           (v & 0b01000000) >> 6,
           (v & 0b00100000) >> 5,
           (v & 0b00010000) >> 4,
           (v & 0b00001000) >> 3,
           (v & 0b00000100) >> 2,
           (v & 0b00000010) >> 1,
           (v & 0b00000001) >> 0
           );
}

