#ifndef CMPZ_CRC32_H
#define CMPZ_CRC32_H

#include <stddef.h>
#include <stdint.h>

/*
 * crc32_update - compute or continue a CRC-32 checksum (the same one used by
 * gzip, zip and PNG).
 *
 * crc  : the checksum so far. Pass 0 to start a new checksum.
 * data : the bytes to add. May be NULL only when size is 0 (then crc is returned unchanged).
 * size : number of bytes in data.
 *
 * Returns the updated checksum. The data can be fed in pieces:
 *   crc32_update(crc32_update(0, a, na), b, nb)  ==  crc32_update(0, a_followed_by_b, na + nb)
 *
 * Known value: the CRC-32 of the 9 bytes "123456789" is 0xCBF43926.
 * Note: a CRC detects accidental damage, it is not a security feature.
 */
uint32_t crc32_update(uint32_t crc, const unsigned char *data, size_t size);

#endif /* CMPZ_CRC32_H */