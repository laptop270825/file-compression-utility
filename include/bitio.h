#ifndef CMPZ_BITIO_H
#define CMPZ_BITIO_H
#include <stddef.h>
#include <stdint.h>

typedef struct {
    unsigned char *buf;      // finished bytes
    size_t size;             // how many bytes in buf are finished
    size_t capacity;         // how many bytes are allocated
    unsigned char cur;       // the byte being built right now
    int nbits;               // how many bits of cur are filled (0 to 7)
} BitWriter;

typedef struct {
    const unsigned char *data;  // the bytes being read (not owned)
    size_t size;                // number of bytes in data
    size_t bit_pos;             // how many bits have been consumed so far
} BitReader;

int  bitwriter_init(BitWriter *bw);

//write_bits writes the lowest nbits bits of value, most significant bit first, with nbits between 1 and 32. It returns 0 on success and -1 on error.
int  bitwriter_write_bits(BitWriter *bw, uint32_t value, int nbits);

//finish pads the last partial byte with zeros, then hands the buffer to the caller, who must free it, and resets the writer
int  bitwriter_finish(BitWriter *bw, unsigned char **data, size_t *size);

//free releases the buffer, for when you abandon a writer without calling finish
void bitwriter_free(BitWriter *bw);

void bitreader_init(BitReader *br, const unsigned char *data, size_t size);

//read_bits returns -1 if fewer than nbits bits remain
int  bitreader_read_bits(BitReader *br, int nbits, uint32_t *value);

#endif // CMPZ_BITIO_H