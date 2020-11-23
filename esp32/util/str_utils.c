#include "str_utils.h"
#include <stdio.h>
#include <string.h>

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

/* hexdump a buffer
*/
void hexdump(const uint8_t* buf, size_t len){
    const size_t line_len = 16;
    uint8_t line[line_len+1];
    memset(line, ' ', line_len);
    line[line_len] = '\0';

    for ( size_t i = 0; i < len; ++i) {
        uint8_t c = buf[i];
        printf("%02x ", c);
        if (   (c >= (uint8_t)'a' &&  c <= (uint8_t)'z')
            || (c >= (uint8_t)'A' &&  c <= (uint8_t)'Z')
            || (c >= (uint8_t)'0' &&  c <= (uint8_t)'9') )
        {
            line[i%line_len] = c;
        } else {
            line[i%line_len] = '.';
        }

        // space after 8 bytes
        if (i%16 == 7)
            printf("  ");
        // end of line after 16 bytes
        if (i%16==15){
            printf("  |%s|\n", line);
            memset(line, ' ', line_len);
        }
    }
    if ( len % line_len ){
        // space after 8 bytes
        if ( len % line_len < 7)
            printf("  ");
        // spaces for bytes we didn't have to print
        for ( size_t j = line_len; j > len % line_len; j-- ){
            printf("   ");
        }
        printf("  |%s|\n", line);
    }
}
