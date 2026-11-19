/* test_container.c - unit tests for the CMPZ container (header, stored mode, validation).
 * Run with: mingw32-make test */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "container.h"

static int failures = 0;
#define CHECK(cond)                                                    \
    do {                                                               \
        if (!(cond)) {                                                 \
            printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);   \
            failures++;                                                \
        }                                                              \
    } while (0)

static uint32_t lcg_state = 4242u;
static uint32_t next_random(void) {
    lcg_state = lcg_state * 1664525u + 1013904223u;
    return lcg_state;
}

/* Compress then decompress; the original must come back unchanged. */
static void round_trip(const unsigned char *data, size_t n) {
    unsigned char *file = NULL, *back = NULL;
    size_t file_size = 0, back_size = 12345;

    CHECK(cmpz_compress(data, n, &file, &file_size) == 0);
    CHECK(file != NULL);
    if (file == NULL) return;
    CHECK(file_size == CMPZ_HEADER_SIZE + n);     /* stored mode: header + raw bytes */
    CHECK(cmpz_decompress(file, file_size, &back, &back_size) == 0);
    CHECK(back != NULL);
    CHECK(back_size == n);
    CHECK(n == 0 || (back != NULL && memcmp(back, data, n) == 0));
    free(file);
    free(back);
}

/* Every byte of the file, checked against docs/FORMAT.md: 20-byte header + the data. */
static void test_exact_bytes(void) {
    const unsigned char text[] = "aaaabbc";
    const unsigned char expected[27] = {
        0x43, 0x4D, 0x50, 0x5A,                          /* magic "CMPZ" */
        0x01,                                            /* version */
        0x01,                                            /* flags: stored */
        0x00, 0x00,                                      /* reserved */
        0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  /* original size 7, little-endian */
        0xC2, 0xAC, 0xEE, 0x9C,                          /* CRC-32 0x9CEEACC2, little-endian */
        'a', 'a', 'a', 'a', 'b', 'b', 'c'
    };
    unsigned char *file = NULL;
    size_t size = 0;

    CHECK(cmpz_compress(text, 7, &file, &size) == 0);
    CHECK(size == 27);
    CHECK(file != NULL && memcmp(file, expected, 27) == 0);
    free(file);
}

static void test_empty_input(void) {
    const unsigned char expected[20] = {
        0x43, 0x4D, 0x50, 0x5A, 0x01, 0x01, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00
    };
    unsigned char *file = NULL;
    size_t size = 0;

    CHECK(cmpz_compress(NULL, 0, &file, &size) == 0);
    CHECK(size == 20);
    CHECK(file != NULL && memcmp(file, expected, 20) == 0);
    free(file);
    round_trip(NULL, 0);
}

static void test_round_trips(void) {
    enum { MAX = 100000 };
    static unsigned char data[MAX];
    size_t sizes[] = {1, 2, 3, 7, 255, 256, 257, 1000, MAX};

    for (size_t i = 0; i < MAX; ++i) data[i] = (unsigned char)(next_random() >> 16);
    for (size_t k = 0; k < sizeof sizes / sizeof sizes[0]; ++k) round_trip(data, sizes[k]);
    memset(data, 0, MAX);
    round_trip(data, MAX);
}

static void test_deterministic(void) {
    const unsigned char text[] = "the same input gives the same file";
    unsigned char *a = NULL, *b = NULL;
    size_t sa = 0, sb = 0;
    CHECK(cmpz_compress(text, sizeof text - 1, &a, &sa) == 0);
    CHECK(cmpz_compress(text, sizeof text - 1, &b, &sb) == 0);
    CHECK(sa == sb && a != NULL && b != NULL && memcmp(a, b, sa) == 0);
    free(a);
    free(b);
}

/* Bad arguments are refused, and the outputs are left exactly as they were. */
static void test_invalid_arguments(void) {
    const unsigned char text[] = "abc";
    unsigned char *sentinel = (unsigned char *)text; /* any recognisable value */
    unsigned char *out = sentinel;
    size_t out_size = 777;

    CHECK(cmpz_compress(NULL, 3, &out, &out_size) == -1);
    CHECK(cmpz_compress(text, 3, NULL, &out_size) == -1);
    CHECK(cmpz_compress(text, 3, &out, NULL) == -1);
    CHECK(cmpz_decompress(NULL, 30, &out, &out_size) == -1);
    CHECK(cmpz_decompress(text, 3, NULL, &out_size) == -1);
    CHECK(cmpz_decompress(text, 3, &out, NULL) == -1);
    CHECK(out == sentinel && out_size == 777);
}

/* Builds a valid file for "hello world" into file[], returns its size. */
static size_t make_file(unsigned char **file) {
    const unsigned char text[] = "hello world";
    size_t size = 0;
    *file = NULL;
    CHECK(cmpz_compress(text, 11, file, &size) == 0);
    return size;
}

/* Runs the decoder and checks that it said no and left the outputs alone. */
static int rejected(const unsigned char *file, size_t size) {
    unsigned char *out = (unsigned char *)"x";
    unsigned char *before = out;
    size_t out_size = 555;
    int r = cmpz_decompress(file, size, &out, &out_size);
    if (r == 0) {
        free(out);   /* accepted: release what it allocated (the caller of rejected() expects 'no') */
        return 0;
    }
    return r == -1 && out == before && out_size == 555;
}

static void test_too_short(void) {
    unsigned char *file;
    size_t size = make_file(&file);
    if (file == NULL) return;
    for (size_t cut = 0; cut < size; ++cut) {    /* every possible truncation */
        CHECK(rejected(file, cut));
    }
    CHECK(rejected(NULL, 0));
    free(file);
}

static void test_bad_header_fields(void) {
    unsigned char *file;
    size_t size = make_file(&file);
    if (file == NULL) return;

    for (int i = 0; i < 4; ++i) {                 /* each magic byte */
        file[i] ^= 0x20;
        CHECK(rejected(file, size));
        file[i] ^= 0x20;
    }
    unsigned char versions[] = {0, 2, 255};
    for (int i = 0; i < 3; ++i) {                 /* unknown versions */
        unsigned char keep = file[4];
        file[4] = versions[i];
        CHECK(rejected(file, size));
        file[4] = keep;
    }
    unsigned char flags[] = {0x02, 0x03, 0x80, 0xFF};
    for (int i = 0; i < 4; ++i) {                 /* unknown flag bits */
        unsigned char keep = file[5];
        file[5] = flags[i];
        CHECK(rejected(file, size));
        file[5] = keep;
    }
    for (int i = 6; i <= 7; ++i) {                /* reserved bytes must stay zero */
        file[i] = 1;
        CHECK(rejected(file, size));
        file[i] = 0;
    }
    CHECK(!rejected(file, size));                 /* and the untouched file is still fine */
    free(file);
}

static void test_size_mismatch(void) {
    unsigned char *file, *bigger;
    size_t size = make_file(&file);
    if (file == NULL) return;

    CHECK(rejected(file, size - 1));              /* a byte missing */

    bigger = malloc(size + 1);                    /* a byte too many */
    CHECK(bigger != NULL);
    if (bigger != NULL) {
        memcpy(bigger, file, size);
        bigger[size] = 0;
        CHECK(rejected(bigger, size + 1));
        free(bigger);
    }

    file[8] = 12;                                 /* declared 12, only 11 present */
    CHECK(rejected(file, size));
    file[8] = 10;                                 /* declared 10, 11 present */
    CHECK(rejected(file, size));

    file[8] = 11;
    file[15] = 0xFF;                              /* declares a gigantic size: refuse, do not allocate */
    CHECK(rejected(file, size));
    memset(file + 8, 0xFF, 8);                    /* the largest possible 64-bit size */
    CHECK(rejected(file, size));
    memset(file + 8, 0, 8);
    file[12] = 1;                                 /* 2^32 + 0: does not fit a 32-bit size_t */
    CHECK(rejected(file, size));
    free(file);
}

static void test_checksum(void) {
    unsigned char *file;
    size_t size = make_file(&file);
    if (file == NULL) return;
    for (size_t i = 16; i < size; ++i) {          /* damage in the CRC field or in the data */
        file[i] ^= 0x01;
        CHECK(rejected(file, size));
        file[i] ^= 0x01;
    }
    free(file);
}

/* The strongest check: flipping ANY single bit anywhere in the file must be noticed. */
static void test_every_bit_flip(void) {
    unsigned char *file;
    size_t size = make_file(&file);
    if (file == NULL) return;
    int undetected = 0;
    for (size_t i = 0; i < size; ++i) {
        for (int bit = 0; bit < 8; ++bit) {
            file[i] ^= (unsigned char)(1u << bit);
            if (!rejected(file, size)) undetected++;
            file[i] ^= (unsigned char)(1u << bit);
        }
    }
    CHECK(undetected == 0);
    free(file);
}

/* Random garbage must never crash; it can only be accepted if it really is a valid file. */
static void test_garbage(void) {
    unsigned char buf[64];
    int accepted = 0;
    for (int round = 0; round < 5000; ++round) {
        size_t len = (size_t)(next_random() >> 16) % (sizeof buf + 1);
        for (size_t i = 0; i < len; ++i) buf[i] = (unsigned char)(next_random() >> 16);
        if (round % 2 == 0 && len >= 8) memcpy(buf, "CMPZ\x01\x01\x00\x00", 8); /* a plausible start */
        unsigned char *out = NULL;
        size_t out_size = 0;
        if (cmpz_decompress(buf, len, &out, &out_size) == 0) {
            accepted++;
            free(out);
        }
    }
    CHECK(accepted == 0);
}

int main(void) {
    test_exact_bytes();
    test_empty_input();
    test_round_trips();
    test_deterministic();
    test_invalid_arguments();
    test_too_short();
    test_bad_header_fields();
    test_size_mismatch();
    test_checksum();
    test_every_bit_flip();
    test_garbage();
    if (failures == 0) { printf("container: all tests passed\n"); return 0; }
    printf("container: %d check(s) FAILED\n", failures);
    return 1;
}