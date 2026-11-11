/* test_bitio.c - unit tests for BitWriter and BitReader.
 * Run with: mingw32-make test */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "bitio.h"

static int failures = 0;

#define CHECK(cond)                                                    \
    do {                                                               \
        if (!(cond)) {                                                 \
            printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);   \
            failures++;                                                \
        }                                                              \
    } while (0)

/* Writes 101, 01, 1100 and expects the bytes AE 00 (the example from the explanation). */
static void test_worked_example(void) {
    BitWriter bw;
    unsigned char *data = NULL;
    size_t size = 0;
    CHECK(bitwriter_init(&bw) == 0);
    CHECK(bitwriter_write_bits(&bw, 0x5, 3) == 0);
    CHECK(bitwriter_write_bits(&bw, 0x1, 2) == 0);
    CHECK(bitwriter_write_bits(&bw, 0xC, 4) == 0);
    CHECK(bitwriter_finish(&bw, &data, &size) == 0);
    CHECK(size == 2);
    if (data != NULL && size == 2) {
        CHECK(data[0] == 0xAE);
        CHECK(data[1] == 0x00);
    }
    free(data);
}

/* 11111 then 000 fills exactly one byte: 11111000 = F8. */
static void test_exact_byte(void) {
    BitWriter bw;
    unsigned char *data = NULL;
    size_t size = 0;
    CHECK(bitwriter_init(&bw) == 0);
    CHECK(bitwriter_write_bits(&bw, 0x1F, 5) == 0);
    CHECK(bitwriter_write_bits(&bw, 0x0, 3) == 0);
    CHECK(bitwriter_finish(&bw, &data, &size) == 0);
    CHECK(size == 1);
    if (data != NULL && size == 1) {
        CHECK(data[0] == 0xF8);
    }
    free(data);
}

/* A partial byte is padded with zeros: 101 becomes 10100000 = A0. */
static void test_padding(void) {
    BitWriter bw;
    unsigned char *data = NULL;
    size_t size = 0;
    CHECK(bitwriter_init(&bw) == 0);
    CHECK(bitwriter_write_bits(&bw, 0x5, 3) == 0);
    CHECK(bitwriter_finish(&bw, &data, &size) == 0);
    CHECK(size == 1);
    if (data != NULL && size == 1) {
        CHECK(data[0] == 0xA0);
    }
    free(data);
}

/* The maximum width, 32 bits, comes out in big-endian order. */
static void test_32_bits(void) {
    BitWriter bw;
    unsigned char *data = NULL;
    size_t size = 0;
    CHECK(bitwriter_init(&bw) == 0);
    CHECK(bitwriter_write_bits(&bw, 0xDEADBEEFu, 32) == 0);
    CHECK(bitwriter_finish(&bw, &data, &size) == 0);
    CHECK(size == 4);
    if (data != NULL && size == 4) {
        CHECK(data[0] == 0xDE);
        CHECK(data[1] == 0xAD);
        CHECK(data[2] == 0xBE);
        CHECK(data[3] == 0xEF);
    }
    free(data);
}

/* Finishing a writer that never wrote anything is valid: zero bytes. */
static void test_empty(void) {
    BitWriter bw;
    unsigned char *data = (unsigned char *)1; /* must be overwritten */
    size_t size = 99;
    CHECK(bitwriter_init(&bw) == 0);
    CHECK(bitwriter_finish(&bw, &data, &size) == 0);
    CHECK(size == 0);
    CHECK(data == NULL);
    free(data);
}

/* Abandoning a writer without finishing must not leak or crash. */
static void test_free_without_finish(void) {
    BitWriter bw;
    CHECK(bitwriter_init(&bw) == 0);
    CHECK(bitwriter_write_bits(&bw, 0xFF, 8) == 0);
    bitwriter_free(&bw);
    bitwriter_free(&bw); /* a second free must be harmless */
    bitwriter_free(NULL);
}

static void test_invalid_arguments(void) {
    BitWriter bw;
    BitReader br;
    unsigned char byte = 0xFF;
    unsigned char *data = NULL;
    size_t size = 0;
    uint32_t v = 0;

    CHECK(bitwriter_init(NULL) == -1);
    CHECK(bitwriter_init(&bw) == 0);
    CHECK(bitwriter_write_bits(&bw, 1, 0) == -1);
    CHECK(bitwriter_write_bits(&bw, 1, 33) == -1);
    CHECK(bitwriter_write_bits(NULL, 1, 1) == -1);
    CHECK(bitwriter_finish(&bw, NULL, &size) == -1);
    CHECK(bitwriter_finish(&bw, &data, NULL) == -1);
    CHECK(bitwriter_finish(NULL, &data, &size) == -1);
    bitwriter_free(&bw);

    bitreader_init(&br, &byte, 1);
    CHECK(bitreader_read_bits(&br, 0, &v) == -1);
    CHECK(bitreader_read_bits(&br, 33, &v) == -1);
    CHECK(bitreader_read_bits(&br, 1, NULL) == -1);
    CHECK(bitreader_read_bits(NULL, 1, &v) == -1);
}

/* Reads back 101, 01, 1100 from AE 00, then the 7 padding bits, then runs dry. */
static void test_read_example(void) {
    const unsigned char bytes[2] = {0xAE, 0x00};
    BitReader br;
    uint32_t v = 0;
    bitreader_init(&br, bytes, 2);
    CHECK(bitreader_read_bits(&br, 3, &v) == 0 && v == 0x5);
    CHECK(bitreader_read_bits(&br, 2, &v) == 0 && v == 0x1);
    CHECK(bitreader_read_bits(&br, 4, &v) == 0 && v == 0xC);
    CHECK(bitreader_read_bits(&br, 7, &v) == 0 && v == 0x0); /* the padding */
    CHECK(bitreader_read_bits(&br, 1, &v) == -1);            /* nothing left */
}

/* A failed read must not move the position. */
static void test_failed_read_keeps_position(void) {
    const unsigned char bytes[1] = {0xFF};
    BitReader br;
    uint32_t v = 0;
    bitreader_init(&br, bytes, 1);
    CHECK(bitreader_read_bits(&br, 9, &v) == -1);
    CHECK(bitreader_read_bits(&br, 8, &v) == 0 && v == 0xFF);
}

/* Reading from an empty buffer fails cleanly. */
static void test_read_empty(void) {
    BitReader br;
    uint32_t v = 0;
    bitreader_init(&br, NULL, 0);
    CHECK(bitreader_read_bits(&br, 1, &v) == -1);
}

/* Simple deterministic pseudo-random numbers, so a failure is reproducible. */
static uint32_t lcg_state = 12345u;
static uint32_t next_random(void) {
    lcg_state = lcg_state * 1664525u + 1013904223u;
    return lcg_state;
}

/* The key test: write 20000 random (value, width) pairs, read them all back. */
static void test_round_trip_random(void) {
    enum { COUNT = 20000 };
    uint32_t *values = malloc(COUNT * sizeof(uint32_t));
    int *widths = malloc(COUNT * sizeof(int));
    CHECK(values != NULL && widths != NULL);
    if (values == NULL || widths == NULL) {
        free(values);
        free(widths);
        return;
    }

    BitWriter bw;
    unsigned char *data = NULL;
    size_t size = 0;
    uint64_t total_bits = 0;
    CHECK(bitwriter_init(&bw) == 0);

    for (int i = 0; i < COUNT; ++i) {
        int w = 1 + (int)((next_random() >> 16) % 32);
        uint32_t mask = (w == 32) ? 0xFFFFFFFFu : ((1u << w) - 1u);
        uint32_t v = next_random();
        widths[i] = w;
        values[i] = v & mask; /* the writer only keeps the low w bits */
        CHECK(bitwriter_write_bits(&bw, v, w) == 0);
        total_bits += (uint64_t)w;
    }
    CHECK(bitwriter_finish(&bw, &data, &size) == 0);
    CHECK((uint64_t)size == (total_bits + 7) / 8);

    BitReader br;
    bitreader_init(&br, data, size);
    int mismatches = 0;
    for (int i = 0; i < COUNT; ++i) {
        uint32_t got = 0;
        if (bitreader_read_bits(&br, widths[i], &got) != 0 || got != values[i]) {
            mismatches++;
        }
    }
    CHECK(mismatches == 0);

    /* The unused bits at the end of the last byte must be zero. */
    uint64_t pad = (uint64_t)size * 8 - total_bits;
    if (pad > 0) {
        uint32_t p = 1;
        CHECK(bitreader_read_bits(&br, (int)pad, &p) == 0 && p == 0);
    }

    free(data);
    free(values);
    free(widths);
}

int main(void) {
    test_worked_example();
    test_exact_byte();
    test_padding();
    test_32_bits();
    test_empty();
    test_free_without_finish();
    test_invalid_arguments();
    test_read_example();
    test_failed_read_keeps_position();
    test_read_empty();
    test_round_trip_random();

    if (failures == 0) {
        printf("bitio: all tests passed\n");
        return 0;
    }
    printf("bitio: %d check(s) FAILED\n", failures);
    return 1;
}