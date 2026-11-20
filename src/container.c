#include "container.h"
#include "crc32.h"

#include <stdlib.h>
#include <string.h>

#include "container.h"
#include "bitio.h"
#include "crc32.h"
#include "freq.h"
#include "huffman.h"

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

/* Fills the 20-byte header at the start of buf. */
static void write_header(unsigned char *buf, unsigned char flags, size_t original, uint32_t crc) {
    // TODO: memcpy magic; buf[4] = version; buf[5] = flags; buf[6] = buf[7] = 0;
    memcpy(buf, MAGIC, 4);
    buf[4] = CMPZ_VERSION;
    buf[5] = flags;
    buf[6] = 0;
    buf[7] = 0;
    //       put_u64(buf + 8, (uint64_t)original); put_u32(buf + 16, crc);
    put_u64(buf + 8, (uint64_t)original);
    put_u32(buf + 16, crc);
}

/*
 * Returns 0 and a new buffer only when the Huffman file is strictly smaller than the
 * stored one. Returns -1 when it is not smaller or when anything goes wrong: the
 * caller then simply uses stored mode, which is always valid.
 */
static int try_huffman(const unsigned char *in, size_t n, unsigned char **out, size_t *out_size) {
    uint64_t freq[HUFF_SYMBOLS];
    HuffTree tree;
    HuffCode codes[HUFF_SYMBOLS], canon[HUFF_SYMBOLS];
    unsigned char lengths[HUFF_SYMBOLS];
    BitWriter bw;
    unsigned char *payload = NULL;
    size_t payload_size = 0;
    int result = -1;

    if (bitwriter_init(&bw) != 0) {
        return -1;
    }
    // One chained condition: each call must return 0, and the first failure stops the chain.
    if (count_frequencies(in, n, freq) != 0 ||
        huffman_build_tree(freq, &tree) != 0 ||
        huffman_build_codes(&tree, codes) != 0 ||
        huffman_code_lengths(codes, lengths) != 0 ||
        huffman_canonical_codes(lengths, canon) != 0 ||      // the same function the reader uses
        huffman_encode(in, n, canon, &bw) != 0 ||
        bitwriter_finish(&bw, &payload, &payload_size) != 0) {
        goto done;
    }

    size_t fixed = CMPZ_HEADER_SIZE + HUFF_SYMBOLS;          // header + length table
    // TODO: if payload_size > SIZE_MAX - fixed -> goto done (overflow)
    if (payload_size > SIZE_MAX - fixed) {
        goto done;
    }
    size_t total = fixed + payload_size;
    // TODO: if total >= CMPZ_HEADER_SIZE + n -> goto done   (use Huffman only if STRICTLY smaller)
    if (total >= CMPZ_HEADER_SIZE + n) {
        goto done;
    }

    unsigned char *buf = malloc(total);
    // TODO: if buf == NULL goto done
    if (buf == NULL) {
        goto done;
    }
    // TODO: write_header(buf, 0, n, crc32_update(0, in, n));      flags 0 = Huffman
    write_header(buf, 0, n, crc32_update(0, in, n));
    // TODO: memcpy(buf + CMPZ_HEADER_SIZE, lengths, HUFF_SYMBOLS);
    memcpy(buf + CMPZ_HEADER_SIZE, lengths, HUFF_SYMBOLS);
    // TODO: memcpy(buf + fixed, payload, payload_size);
    memcpy(buf + fixed, payload, payload_size);
    *out = buf;
    *out_size = total;
    result = 0;

done:
    bitwriter_free(&bw);       // safe even after a successful finish
    free(payload);
    return result;
}

/*int cmpz_compress(const unsigned char *in, size_t in_size,
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
    if (in_size > 0 && try_huffman(in, in_size, out, out_size) == 0) {
        return 0; // Huffman succeeded and is smaller than stored
    } else {
        write_header(buf, CMPZ_FLAG_STORED, in_size, crc32_update(0, in, in_size));
        if (in_size > 0) {
            memcpy(buf + CMPZ_HEADER_SIZE, in, in_size);
        }
    }

    // Only now, when everything succeeded, touch the outputs.
    *out = buf;
    *out_size = total;
    return 0;
}*/

int cmpz_compress(const unsigned char *in, size_t in_size,
                  unsigned char **out, size_t *out_size) {
    if (out == NULL || out_size == NULL || (in == NULL && in_size > 0)) {
        return -1; // invalid parameters
    }
    if (in_size > SIZE_MAX - CMPZ_HEADER_SIZE) {
        return -1; // the total size would overflow
    }

    // Huffman mode when it makes the file smaller (never for an empty input).
    if (in_size > 0 && try_huffman(in, in_size, out, out_size) == 0) {
        return 0;
    }

    // Otherwise stored mode: header + the original bytes.
    size_t total = CMPZ_HEADER_SIZE + in_size;
    unsigned char *buf = malloc(total);
    if (buf == NULL) {
        return -1; // out of memory
    }
    write_header(buf, CMPZ_FLAG_STORED, in_size, crc32_update(0, in, in_size));
    if (in_size > 0) {
        memcpy(buf + CMPZ_HEADER_SIZE, in, in_size);
    }
    *out = buf;
    *out_size = total;
    return 0;
}

static int decode_huffman(const unsigned char *in, size_t in_size, uint64_t original,
                          uint32_t stored_crc, unsigned char **out, size_t *out_size) {
    size_t fixed = CMPZ_HEADER_SIZE + HUFF_SYMBOLS;
    // TODO: if in_size <= fixed return -1                (no room for even one payload byte)
    if (in_size <= fixed) {
        return -1;
    }
    size_t payload_size = in_size - fixed;
    const unsigned char *payload = in + fixed;

    // TODO: if original == 0 return -1                   (an empty original is always stored)
    if (original == 0) {
        return -1;
    }
    // TODO: if original > (uint64_t)payload_size * 8 return -1   (decompression-bomb guard)
    if (original > (uint64_t)payload_size * 8) {
        return -1;
    }
    // TODO: if original > (uint64_t)SIZE_MAX return -1
    if (original > (uint64_t)SIZE_MAX) {
        return -1;
    }
    size_t n = (size_t)original;

    HuffCode codes[HUFF_SYMBOLS];
    HuffTree tree;
    // TODO: if huffman_canonical_codes(in + CMPZ_HEADER_SIZE, codes) != 0 ||
    //          huffman_tree_from_codes(codes, &tree) != 0   return -1
    if (huffman_canonical_codes(in + CMPZ_HEADER_SIZE, codes) != 0 ||
        huffman_tree_from_codes(codes, &tree) != 0) {
        return -1;
    }

    unsigned char *buf = malloc(n);
    // TODO: if buf == NULL return -1
    if (buf == NULL) {
        return -1;
    }
    BitReader br;
    bitreader_init(&br, payload, payload_size);
    // TODO: if huffman_decode(&br, &tree, n, buf) != 0 -> free(buf); return -1
    if (huffman_decode(&br, &tree, n, buf) != 0) {
        free(buf);
        return -1;
    }

    // The payload must be used completely, and the padding bits must be zero.
    // TODO: if (br.bit_pos + 7) / 8 != (uint64_t)payload_size -> free(buf); return -1
    if ((br.bit_pos + 7) / 8 != (uint64_t)payload_size) {
        free(buf);
        return -1;
    }
    int pad = (int)(br.bit_pos % 8);
    if (pad != 0) {
        uint32_t padding = 0;
        // TODO: read the remaining (8 - pad) bits with bitreader_read_bits(&br, 8 - pad, &padding);
        //       if it fails or padding != 0 -> free(buf); return -1
        if (bitreader_read_bits(&br, 8 - pad, &padding) != 0 || padding != 0) {
            free(buf);
            return -1;
        }
    }
    // TODO: if crc32_update(0, buf, n) != stored_crc -> free(buf); return -1
    if (crc32_update(0, buf, n) != stored_crc) {
        free(buf);
        return -1;
    }

    *out = buf;
    *out_size = n;
    return 0;
}

static int decode_stored(const unsigned char *in, size_t in_size, uint64_t original,
                         uint32_t stored_crc, unsigned char **out, size_t *out_size) {
    // TODO: exactly your 4d-1 code from "original != in_size - 20" to the end
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
    // TODO: Implement the rest of the decode_stored function
}

int cmpz_decompress(const unsigned char *in, size_t in_size,
                    unsigned char **out, size_t *out_size) {
    if (out == NULL || out_size == NULL || in == NULL) {
        return -1; // invalid parameters
    }
    if (in_size < CMPZ_HEADER_SIZE) {
        return -1; // too short to hold a header
    }
    if (memcmp(in, MAGIC, 4) != 0) {
        return -1; // not a CMPZ file
    }
    if (in[4] != CMPZ_VERSION) {
        return -1; // a version this program does not know
    }
    if (in[5] != 0 && in[5] != CMPZ_FLAG_STORED) {
        return -1; // unknown flag bits
    }
    if (in[6] != 0 || in[7] != 0) {
        return -1; // reserved bytes must be zero
    }

    uint64_t original = get_u64(in + 8);
    uint32_t stored_crc = get_u32(in + 16);

    if (in[5] == CMPZ_FLAG_STORED) {
        return decode_stored(in, in_size, original, stored_crc, out, out_size);
    }
    return decode_huffman(in, in_size, original, stored_crc, out, out_size);
}