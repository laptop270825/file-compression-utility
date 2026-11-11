#ifndef CMPZ_FREQ_H
#define CMPZ_FREQ_H

#include <stddef.h> // for size_t
#include <stdint.h> // for uint32_t

/*
 * count_frequencies - count how many times each byte value appears.
 *
 * data : the bytes to count. May be NULL only when size is 0.
 * size : the number of bytes in data. 0 is valid (an empty file).
 * freq : output table of 256 counters, one per possible byte value.
 *        After the call, freq[b] is the number of times the byte b
 *        appears in data. The function resets the whole table to zero
 *        itself, so the caller does not need to initialise it.
 *
 * Returns 0 on success, or -1 on failure (freq is NULL, or data is NULL
 * while size > 0). Empty input is not an error: every count is zero.
 */
int count_frequencies(const unsigned char *data, size_t size, uint64_t freq[256]);

#endif /* CMPZ_FREQ_H */