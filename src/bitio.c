#include <stdlib.h>
#include "bitio.h"

static int push_byte(BitWriter *bw, unsigned char byte) {
    if (bw->size >= bw->capacity) {
        size_t new_capacity = bw->capacity == 0 ? 64 : bw->capacity * 2;
        unsigned char *new_buf = realloc(bw->buf, new_capacity);
        if (!new_buf) {
            return -1; // Memory allocation failed
        }
        bw->buf = new_buf;
        bw->capacity = new_capacity;
    }
    bw->buf[bw->size++] = byte;
    return 0; // Success
}

int bitwriter_init(BitWriter *bw) {
    if (!bw) {
        return -1; // Error: bw is NULL
    }
    bw->buf = NULL;
    bw->size = 0;
    bw->capacity = 0;
    bw->cur = 0;
    bw->nbits = 0;
    return 0; // Success
}

int bitwriter_write_bits(BitWriter *bw, uint32_t value, int nbits) {
    if (!bw || nbits < 1 || nbits > 32) {
        return -1; // Error: invalid parameters
    }
    for (int i = nbits - 1; i >= 0; --i) {
        unsigned bit = (value >> i) & 1u;
        bw->cur = (unsigned char)((bw->cur << 1) | bit);
        bw->nbits++;
        if (bw->nbits == 8) {
            if (push_byte(bw, bw->cur) != 0) {
                return -1; // Error: failed to push byte
            }
            bw->cur = 0;
            bw->nbits = 0;
        }
    }
    return 0; // Success
}

int bitwriter_finish(BitWriter *bw, unsigned char **data, size_t *size) {
    if (!bw || !data || !size) {
        return -1; // Error: invalid parameters
    }
    if (bw->nbits > 0) {
        unsigned char last = (unsigned char)(bw->cur << (8 - bw->nbits)); // Pad with zeros
        if (push_byte(bw, last) != 0) {
            return -1; // Error: failed to push byte
        }
    }
    *data = bw->buf;
    *size = bw->size;
    bw->buf = NULL; // Ownership moves to the caller
    bw->size = 0;
    bw->capacity = 0;
    bw->cur = 0;
    bw->nbits = 0;
    return 0; // Success
}

void bitwriter_free(BitWriter *bw) {
    if (!bw) {
        return;
    }
    free(bw->buf);
    bw->buf = NULL;
    bw->size = 0;
    bw->capacity = 0;
    bw->cur = 0;
    bw->nbits = 0;
}