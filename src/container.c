#include "container.h"
#include "crc32.h"

#include <stdlib.h>
#include <string.h>

static const unsigned char MAGIC[4] = {'C', 'M', 'P', 'Z'};

/* Integers are written one byte at a time, lowest byte first (little-endian),
 * so the file is identical on every computer. */
static void put_u32(unsigned char *p, uint32_t v) {
    for (int i = 0; i < 4; ++i) {
        // TODO: p[i] = (unsigned char)(v >> (8 * i));
        p[i] = (unsigned char)(v >> (8 * i));
    }
}

static void put_u64(unsigned char *p, uint64_t v) {
    for (int i = 0; i < 8; ++i) {
        // TODO: same idea, with 8 bytes
        p[i] = (unsigned char)(v >> (8 * i));
    }
}

static uint32_t get_u32(const unsigned char *p) {
    uint32_t v = 0;
    for (int i = 3; i >= 0; --i) {       // highest byte first
        // TODO: v = (v << 8) | p[i];
        v = (v << 8) | p[i];
    }
    return v;
}

static uint64_t get_u64(const unsigned char *p) {
    uint64_t v = 0;
    for (int i = 7; i >= 0; --i) {
        // TODO: v = (v << 8) | p[i];
        v = (v << 8) | p[i];
    }
    return v;
}

int cmpz_compress(const unsigned char *in, size_t in_size,
                  unsigned char **out, size_t *out_size) {
    // TODO: return -1 if out or out_size is NULL, or (in == NULL && in_size > 0)
    if (out == NULL || out_size == NULL || (in == NULL && in_size > 0)) {
        return -1;
    }
    // TODO: return -1 if in_size > SIZE_MAX - CMPZ_HEADER_SIZE   (the total would overflow)
    if (in_size > SIZE_MAX - CMPZ_HEADER_SIZE) {
        return -1;
    }

    size_t total = CMPZ_HEADER_SIZE + in_size;
    unsigned char *buf = malloc(total);
    // TODO: if buf is NULL return -1
    if (buf == NULL) {
        return -1;
    }

    // The header, byte by byte:
    // TODO: memcpy(buf, MAGIC, 4);
    memcpy(buf, MAGIC, 4);
    // TODO: buf[4] = CMPZ_VERSION;  buf[5] = CMPZ_FLAG_STORED;  buf[6] = 0;  buf[7] = 0;
    buf[4] = CMPZ_VERSION;
    buf[5] = CMPZ_FLAG_STORED;
    buf[6] = 0;
    buf[7] = 0;
    // TODO: put_u64(buf + 8, (uint64_t)in_size);
    put_u64(buf + 8, (uint64_t)in_size);
    // TODO: put_u32(buf + 16, crc32_update(0, in, in_size));
    put_u32(buf + 16, crc32_update(0, in, in_size));
    // TODO: the data after the header: if in_size > 0, memcpy(buf + CMPZ_HEADER_SIZE, in, in_size)
    if (in_size > 0) {
        memcpy(buf + CMPZ_HEADER_SIZE, in, in_size);
    }

    // Only now, when everything succeeded, touch the outputs.
    *out = buf;
    *out_size = total;
    return 0;
}

int cmpz_decompress(const unsigned char *in, size_t in_size,
                    unsigned char **out, size_t *out_size) {
    // TODO: NULL checks (same as compress)
    if (out == NULL || out_size == NULL || in == NULL) {
        return -1;
    }
    // TODO: if in_size < CMPZ_HEADER_SIZE return -1
    if (in_size < CMPZ_HEADER_SIZE) {
        return -1;
    }
    // TODO: if memcmp(in, MAGIC, 4) != 0 return -1
    if (memcmp(in, MAGIC, 4) != 0) {
        return -1;
    }
    // TODO: if in[4] != CMPZ_VERSION return -1
    if (in[4] != CMPZ_VERSION) {
        return -1;
    }
    // TODO: if in[5] != CMPZ_FLAG_STORED return -1     (unknown flags, or a mode we don't support yet)
    if (in[5] != CMPZ_FLAG_STORED) {
        return -1;
    }
    // TODO: if in[6] != 0 || in[7] != 0 return -1      (reserved must be zero)
    if (in[6] != 0 || in[7] != 0) {
        return -1;
    }

    uint64_t original = get_u64(in + 8);
    uint32_t stored_crc = get_u32(in + 16);

    // Stored mode: exactly `original` bytes must follow the header.
    // TODO: if original != (uint64_t)(in_size - CMPZ_HEADER_SIZE) return -1
    if (original != (uint64_t)(in_size - CMPZ_HEADER_SIZE)) {
        return -1;
    }
    size_t n = (size_t)original;     // safe now: it equals in_size - 20, so it fits

    unsigned char *buf = malloc(n > 0 ? n : 1);     // never ask malloc for 0 bytes
    // TODO: if buf is NULL return -1
    if (buf == NULL) {
        return -1;
    }
    // TODO: if n > 0, memcpy(buf, in + CMPZ_HEADER_SIZE, n)
    if (n > 0) {
        memcpy(buf, in + CMPZ_HEADER_SIZE, n);
    }
    // TODO: if crc32_update(0, buf, n) != stored_crc  -> free(buf) and return -1
    if (crc32_update(0, buf, n) != stored_crc) {
        free(buf);
        return -1;
    }
    
    *out = buf;
    *out_size = n;
    return 0;
}