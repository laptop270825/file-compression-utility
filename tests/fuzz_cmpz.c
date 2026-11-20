/* fuzz_cmpz.c - a fuzzer for the CMPZ container.
 *
 * Run with:  mingw32-make fuzz            (100000 inputs)
 *            mingw32-make fuzz FUZZ_N=500000
 *
 * The rule it enforces: for ANY input, cmpz_decompress either returns a correct result or
 * says no (-1). It must never crash, hang, or touch memory it does not own. Run it under
 * AddressSanitizer + UBSan on Linux so that silent memory errors become loud ones. */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "bitio.h"
#include "container.h"
#include "crc32.h"
#include "huffman.h"

#define MAX_FILE 70000
#define HEADER 20
#define TABLE 256

/* ---------- random numbers: fixed seed, so every failure can be reproduced ---------- */

static uint64_t rng_state = 88172645463325252ULL;

static uint64_t rnd(void) {                      /* xorshift64* */
    uint64_t x = rng_state;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    rng_state = x;
    return x * 2685821657736338717ULL;
}

static uint32_t below(uint32_t n) {              /* a number in 0 .. n-1 (0 when n is 0) */
    return n == 0 ? 0 : (uint32_t)(rnd() >> 33) % n;
}

/* ---------- little-endian helpers, written independently of container.c ---------- */

static void set_u64(unsigned char *p, uint64_t v) {
    for (int i = 0; i < 8; ++i) p[i] = (unsigned char)(v >> (8 * i));
}
static void set_u32(unsigned char *p, uint32_t v) {
    for (int i = 0; i < 4; ++i) p[i] = (unsigned char)(v >> (8 * i));
}
static uint64_t read_u64(const unsigned char *p) {
    uint64_t v = 0;
    for (int i = 7; i >= 0; --i) v = (v << 8) | p[i];
    return v;
}
static uint32_t read_u32(const unsigned char *p) {
    uint32_t v = 0;
    for (int i = 3; i >= 0; --i) v = (v << 8) | p[i];
    return v;
}

/* ---------- bookkeeping ---------- */

static unsigned long count_rejected = 0, count_accepted = 0, count_constructed = 0, count_roundtrip = 0;
static int failures = 0;

static void fail(const char *why, const unsigned char *input, size_t len) {
    char name[64];
    printf("  FAIL: %s (input of %llu bytes)\n", why, (unsigned long long)len);
    snprintf(name, sizeof name, "build/fuzz_fail_%d.bin", failures);
    FILE *f = fopen(name, "wb");
    if (f != NULL) {
        if (len > 0 && input != NULL) fwrite(input, 1, len, f);
        fclose(f);
        printf("        saved to %s\n", name);
    }
    failures++;
    if (failures >= 10) {
        printf("too many failures, stopping\n");
        exit(1);
    }
}

/* ---------- the checks ---------- */

static unsigned char sentinel_byte;

/* Feeds one input to the decompressor and checks everything we know must be true. */
static void check_input(const unsigned char *buf, size_t len) {
    unsigned char *out = &sentinel_byte;
    size_t out_size = 777;
    int r = cmpz_decompress(buf, len, &out, &out_size);

    if (r == -1) {
        if (out != &sentinel_byte || out_size != 777) fail("outputs changed although it said no", buf, len);
        count_rejected++;
        return;
    }
    if (r != 0) {
        fail("unexpected return value", buf, len);
        return;
    }
    count_accepted++;

    /* Accepted: the result must agree with the header the file claims. */
    if (len < HEADER || memcmp(buf, "CMPZ", 4) != 0) {
        fail("accepted a file without a valid header", buf, len);
    } else {
        if ((uint64_t)out_size != read_u64(buf + 8)) fail("restored size differs from the header", buf, len);
        if (out == NULL || crc32_update(0, out, out_size) != read_u32(buf + 16)) {
            fail("accepted data whose CRC does not match the header", buf, len);
        }
    }

    /* And it must survive being compressed and restored again. */
    if (out != NULL) {
        unsigned char *again = NULL, *back = NULL;
        size_t again_size = 0, back_size = 0;
        if (cmpz_compress(out, out_size, &again, &again_size) != 0 ||
            cmpz_decompress(again, again_size, &back, &back_size) != 0 ||
            back_size != out_size || (out_size > 0 && memcmp(back, out, out_size) != 0)) {
            fail("accepted data does not survive compress + decompress", buf, len);
        }
        free(again);
        free(back);
    }
    free(out);
}

/* ---------- source material: valid files made by the real compressor ---------- */

typedef struct {
    unsigned char *data;
    size_t len;
} Blob;

static Blob corpus[16];
static int corpus_count = 0;

static void add_to_corpus(const unsigned char *plain, size_t n) {
    unsigned char *file = NULL;
    size_t file_size = 0;
    if (cmpz_compress(plain, n, &file, &file_size) == 0) {
        corpus[corpus_count].data = file;
        corpus[corpus_count].len = file_size;
        corpus_count++;
    }
}

static void build_corpus(void) {
    static unsigned char buf[20000];
    const char *sentence = "the quick brown fox jumps over the lazy dog. ";
    size_t sl = strlen(sentence);

    for (size_t i = 0; i < 18000; ++i) buf[i] = (unsigned char)sentence[i % sl];
    add_to_corpus(buf, 18000);                                    /* text */
    for (size_t i = 0; i < 401; ++i) buf[i] = (unsigned char)"aaaaaaabbbc"[i % 11];
    add_to_corpus(buf, 401);                                      /* tiny alphabet */
    for (int round = 0; round < 3; ++round) {
        uint32_t alphabet = 2u + below(120);
        size_t n = 500 + below(15000);
        for (size_t i = 0; i < n; ++i) {
            uint32_t r = below(1000);
            buf[i] = (unsigned char)((r * r / 1000 * alphabet) / 1000);
        }
        add_to_corpus(buf, n);                                    /* skewed */
    }
    memset(buf, 'z', 2000);
    add_to_corpus(buf, 2000);                                     /* one value */
    for (size_t i = 0; i < 2000; ++i) buf[i] = (unsigned char)below(256);
    add_to_corpus(buf, 2000);                                     /* random: stored mode */
    add_to_corpus(NULL, 0);                                       /* empty */
    add_to_corpus(buf, 1);                                        /* one byte */
}

/* ---------- strategy 1: damage a valid file ---------- */

static void mutate(unsigned char *b, size_t *plen, size_t cap) {
    size_t len = *plen;
    int steps = 1 + (int)below(4);

    for (int s = 0; s < steps; ++s) {
        switch (below(9)) {
        case 0:                                                   /* flip one bit */
            if (len > 0) b[below((uint32_t)len)] ^= (unsigned char)(1u << below(8));
            break;
        case 1:                                                   /* random byte */
            if (len > 0) b[below((uint32_t)len)] = (unsigned char)below(256);
            break;
        case 2: {                                                 /* extreme byte */
            const unsigned char extremes[] = {0x00, 0x01, 0x7F, 0x80, 0xFF};
            if (len > 0) b[below((uint32_t)len)] = extremes[below(5)];
            break;
        }
        case 3:                                                   /* truncate */
            len = below((uint32_t)len + 1);
            break;
        case 4: {                                                 /* append garbage */
            uint32_t extra = 1 + below(20);
            for (uint32_t k = 0; k < extra && len < cap; ++k) b[len++] = (unsigned char)below(256);
            break;
        }
        case 5:                                                   /* copy a chunk over another place */
            if (len > 2) {
                size_t from = below((uint32_t)len), to = below((uint32_t)len);
                size_t n = 1 + below((uint32_t)(len - (from > to ? from : to)));
                memmove(b + to, b + from, n);
            }
            break;
        case 6:                                                   /* swap two bytes */
            if (len > 1) {
                size_t x = below((uint32_t)len), y = below((uint32_t)len);
                unsigned char t = b[x];
                b[x] = b[y];
                b[y] = t;
            }
            break;
        case 7:                                                   /* an extreme value in the size field */
            if (len >= HEADER) {
                uint64_t choices[] = {0, 1, 0xFFFFFFFFull, 0x100000000ull, 0x7FFFFFFFull,
                                      0xFFFFFFFFFFFFFFFFull, (uint64_t)len * 8, (uint64_t)len * 8 + 1,
                                      (uint64_t)len, (uint64_t)len - HEADER};
                set_u64(b + 8, choices[below(10)]);
            }
            break;
        case 8:                                                   /* nudge the size field */
            if (len >= HEADER) {
                uint64_t v = read_u64(b + 8);
                uint32_t d = 1 + below(3);
                set_u64(b + 8, below(2) ? v + d : v - d);
            }
            break;
        }
    }
    *plen = len;
}

/* ---------- strategy 2: a correct header, then garbage behind it ---------- */

static size_t structured_garbage(unsigned char *b) {
    size_t payload = 1 + below(300);
    size_t len = HEADER + TABLE + payload;

    memcpy(b, "CMPZ", 4);
    b[4] = 1;
    b[5] = 0;                                                     /* Huffman mode */
    b[6] = b[7] = 0;
    set_u64(b + 8, 1 + below((uint32_t)(payload * 8)));
    set_u32(b + 16, (uint32_t)rnd());

    memset(b + HEADER, 0, TABLE);
    switch (below(3)) {
    case 0:                                                       /* 256 random bytes */
        for (int i = 0; i < TABLE; ++i) b[HEADER + i] = (unsigned char)below(256);
        break;
    case 1: {                                                     /* a few small lengths */
        uint32_t k = 2 + below(40);
        for (uint32_t j = 0; j < k; ++j) b[HEADER + below(TABLE)] = (unsigned char)(1 + below(12));
        break;
    }
    default:                                                      /* a table that is nearly complete */
        for (int i = 0; i < TABLE; ++i) b[HEADER + i] = 8;
        b[HEADER + below(TABLE)] = (unsigned char)below(3) ? 0 : 9;
        break;
    }
    for (size_t i = 0; i < payload; ++i) b[HEADER + TABLE + i] = (unsigned char)below(256);
    return len;
}

/* ---------- strategy 3: files that are VALID but that our compressor would never write ----------
 * A random but legal code table, random payload bits, decoded with the library, and the header
 * filled in to match. The decompressor must accept every one and return exactly those bytes. */

static int build_valid(unsigned char *file, size_t *file_len, unsigned char *expect, size_t *expect_len) {
    uint64_t freq[HUFF_SYMBOLS] = {0};
    HuffTree tree, tree2;
    HuffCode codes[HUFF_SYMBOLS], canon[HUFF_SYMBOLS];
    unsigned char lengths[HUFF_SYMBOLS], payload[300];
    int distinct = 0;

    uint32_t k = 2 + below(255);
    for (uint32_t j = 0; j < k; ++j) {
        uint32_t sym = below(256);
        freq[sym] += 1 + (rnd() % (1u << below(20)));
    }
    for (int i = 0; i < HUFF_SYMBOLS; ++i) if (freq[i] > 0) distinct++;
    if (distinct < 2) return -1;

    if (huffman_build_tree(freq, &tree) != 0 || huffman_build_codes(&tree, codes) != 0 ||
        huffman_code_lengths(codes, lengths) != 0 || huffman_canonical_codes(lengths, canon) != 0 ||
        huffman_tree_from_codes(canon, &tree2) != 0) {
        return -1;
    }

    size_t pl = 1 + below(300);
    for (size_t i = 0; i < pl; ++i) payload[i] = (unsigned char)below(256);

    BitReader br;
    bitreader_init(&br, payload, pl);
    size_t n = 0;
    while (n < 600) {
        BitReader saved = br;
        unsigned char c;
        if (huffman_decode(&br, &tree2, 1, &c) != 0) {
            br = saved;
            break;
        }
        expect[n++] = c;
    }
    if (n == 0) return -1;

    size_t used = (size_t)((br.bit_pos + 7) / 8);
    unsigned r = (unsigned)(br.bit_pos % 8);
    if (r != 0) payload[used - 1] &= (unsigned char)(0xFFu << (8 - r));     /* padding bits must be zero */

    memcpy(file, "CMPZ", 4);
    file[4] = 1;
    file[5] = 0;
    file[6] = file[7] = 0;
    set_u64(file + 8, n);
    set_u32(file + 16, crc32_update(0, expect, n));
    memcpy(file + HEADER, lengths, TABLE);
    memcpy(file + HEADER + TABLE, payload, used);
    *file_len = HEADER + TABLE + used;
    *expect_len = n;
    return 0;
}

static void check_constructed(void) {
    static unsigned char file[MAX_FILE], expect[700];
    size_t flen = 0, elen = 0;
    if (build_valid(file, &flen, expect, &elen) != 0) return;

    unsigned char *out = NULL;
    size_t out_size = 0;
    if (cmpz_decompress(file, flen, &out, &out_size) != 0) {
        fail("rejected a valid file", file, flen);
        return;
    }
    if (out_size != elen || memcmp(out, expect, elen) != 0) fail("a valid file restored to different bytes", file, flen);
    free(out);
    count_constructed++;
}

/* ---------- strategy 4: the compressor itself, on many shapes of data ---------- */

static void check_round_trip(void) {
    static unsigned char data[6000];
    size_t n = below(6000);

    switch (below(6)) {
    case 0: for (size_t i = 0; i < n; ++i) data[i] = (unsigned char)below(256); break;           /* random */
    case 1: { uint32_t a = 1 + below(255); for (size_t i = 0; i < n; ++i) data[i] = (unsigned char)(below(a) * below(a) / a); break; }
    case 2: { size_t i = 0; while (i < n) { unsigned char v = (unsigned char)below(256); size_t run = 1 + below(400); while (run-- && i < n) data[i++] = v; } break; }
    case 3: memset(data, (int)below(256), n); break;                                              /* one value */
    case 4: for (size_t i = 0; i < n; ++i) data[i] = (unsigned char)(i * 7); break;               /* a pattern */
    default: for (size_t i = 0; i < n; ++i) data[i] = (unsigned char)(32 + below(5)); break;      /* tiny alphabet */
    }

    unsigned char *file = NULL, *file2 = NULL, *back = NULL;
    size_t fs = 0, fs2 = 0, bs = 0;
    if (cmpz_compress(data, n, &file, &fs) != 0) { fail("compress failed", data, n); return; }
    if (fs > n + HEADER) fail("the output grew by more than the header", data, n);
    if (cmpz_decompress(file, fs, &back, &bs) != 0 || bs != n || (n > 0 && memcmp(back, data, n) != 0)) {
        fail("round trip failed", data, n);
    }
    if (cmpz_compress(data, n, &file2, &fs2) != 0 || fs2 != fs || memcmp(file, file2, fs) != 0) {
        fail("compression is not deterministic", data, n);
    }
    free(file);
    free(file2);
    free(back);
    count_roundtrip++;
}

/* ---------- main loop ---------- */

int main(int argc, char *argv[]) {
    unsigned long iterations = 100000;
    static unsigned char buf[MAX_FILE];

    if (argc >= 2) iterations = strtoul(argv[1], NULL, 10);
    if (argc >= 3) rng_state ^= strtoull(argv[2], NULL, 10) * 0x9E3779B97F4A7C15ULL;
    if (rng_state == 0) rng_state = 1;

    build_corpus();
    printf("fuzzing %lu inputs (%d valid files as source material)\n", iterations, corpus_count);

    for (unsigned long it = 0; it < iterations; ++it) {
        uint32_t which = below(100);
        if (which < 10) {                                            /* pure random bytes */
            size_t len = below(600);
            for (size_t i = 0; i < len; ++i) buf[i] = (unsigned char)below(256);
            check_input(buf, len);
        } else if (which < 65) {                                     /* damaged valid file */
            Blob *src = &corpus[below((uint32_t)corpus_count)];
            size_t len = src->len;
            memcpy(buf, src->data, len);
            mutate(buf, &len, MAX_FILE);
            check_input(buf, len);
        } else if (which < 80) {                                     /* right header, garbage behind it */
            check_input(buf, structured_garbage(buf));
        } else if (which < 92) {                                     /* valid but unusual files */
            check_constructed();
        } else {                                                     /* compress and restore */
            check_round_trip();
        }
    }

    printf("done: %lu rejected, %lu accepted, %lu constructed-valid files restored, %lu round trips\n",
           count_rejected, count_accepted, count_constructed, count_roundtrip);
    if (failures == 0) { printf("fuzz: no failures\n"); return 0; }
    printf("fuzz: %d failure(s)\n", failures);
    return 1;
}