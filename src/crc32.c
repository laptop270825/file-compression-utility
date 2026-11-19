#include "crc32.h"

#define CRC32_POLY 0xEDB88320u

static uint32_t table[256];
static int table_ready = 0;

/* Fills table[b] with the CRC of the single byte b. */
static void build_table(void) {
    for (uint32_t i = 0; i < 256; ++i) {
        uint32_t c = i;
        for (int j = 0; j < 8; ++j) {
            if (c & 1) {
                c = (c >> 1) ^ CRC32_POLY;
            } else {
                c >>= 1;
            }
        }
        /* TODO 1: repeat 8 times: if lowest bit of c is 1 -> (c >> 1) ^ CRC32_POLY, else c >> 1 */
        table[i] = c;
    }
    table_ready = 1;
}

uint32_t crc32_update(uint32_t crc, const unsigned char *data, size_t size) {
    /* TODO 2: if data is NULL, return crc unchanged */
    if (data == NULL) {
        return crc;
    }
    if (data == NULL && size > 0) {
        return crc; // Invalid case: data is NULL but size is greater than 0
    }
    if (!table_ready) {
        build_table();
    }
    crc = ~crc;    
    for (size_t i = 0; i < size; ++i) {
        crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    /* TODO 3: crc = ~crc; loop over the bytes with
       crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);   then return ~crc */

    return ~crc;
}