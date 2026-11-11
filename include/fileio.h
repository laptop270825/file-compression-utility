#ifndef CMPZ_FILEIO_H
#define CMPZ_FILEIO_H

#include <stddef.h>

/*
 * read_file - read an entire file into memory as raw bytes.
 *
 * path : the file to read (opened in binary mode).
 * data : output. On success, *data points to a buffer holding the file's
 *        bytes. The buffer is allocated with malloc, so the CALLER must
 *        release it with free().
 * size : output. On success, *size is the number of bytes read.
 *        An empty file is valid: *size is 0, and *data is still a valid
 *        pointer that must be freed.
 *
 * Returns 0 on success, or -1 on failure (bad arguments, file cannot be
 * opened or read, out of memory). On failure, *data and *size are left
 * unchanged.
 *
 * Limitation: on Windows, files larger than about 2 GB are rejected,
 * because ftell returns a 32-bit long.
 */
int read_file(const char *path, unsigned char **data, size_t *size);

/*
 * write_file - write a buffer to a file, replacing it if it exists.
 *
 * path : the file to create (opened in binary mode).
 * data : the bytes to write. May be NULL only when size is 0.
 * size : the number of bytes to write. 0 creates an empty file.
 *
 * Returns 0 on success, or -1 on failure (bad arguments, file cannot be
 * created, write error, or error while closing, such as a full disk).
 */
int write_file(const char *path, const unsigned char *data, size_t size);

#endif /* CMPZ_FILEIO_H */