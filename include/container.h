#ifndef CMPZ_CONTAINER_H
#define CMPZ_CONTAINER_H

#include <stddef.h>
#include <stdint.h>

/* The CMPZ file format, version 1 (see docs/FORMAT.md). */
#define CMPZ_HEADER_SIZE  20   /* magic(4) version(1) flags(1) reserved(2) original size(8) crc(4) */
#define CMPZ_VERSION      1
#define CMPZ_FLAG_STORED  0x01 /* the data after the header is the original bytes, uncompressed */

/*
 * cmpz_compress - turn a buffer into a complete CMPZ file in memory.
 *
 * in       : the original bytes. May be NULL only when in_size is 0.
 * in_size  : number of bytes in `in`.
 * out      : receives a newly allocated buffer holding the whole file. The
 *            caller frees it with free(). Never NULL on success.
 * out_size : receives the size of that buffer.
 *
 * Returns 0 on success, -1 on a NULL argument, a size that would overflow, or
 * when memory runs out. On failure *out and *out_size are left unchanged.
 * The output is always the same for the same input.
 */
int cmpz_compress(const unsigned char *in, size_t in_size,
                  unsigned char **out, size_t *out_size);

/*
 * cmpz_decompress - check a CMPZ file held in memory and recover the original bytes.
 *
 * in       : the whole file. Untrusted: every field is validated.
 * in_size  : its size in bytes.
 * out      : receives a newly allocated buffer with the original bytes. The caller
 *            frees it with free(). Never NULL on success, even for an empty original.
 * out_size : receives the number of original bytes.
 *
 * Returns 0 on success. Returns -1 when the file is not a valid CMPZ file: too short,
 * wrong magic or version, unknown flags, non-zero reserved bytes, a size that does not
 * match, a checksum mismatch, and so on. On failure *out and *out_size are left
 * unchanged and nothing is leaked.
 */
int cmpz_decompress(const unsigned char *in, size_t in_size,
                    unsigned char **out, size_t *out_size);

#endif /* CMPZ_CONTAINER_H */