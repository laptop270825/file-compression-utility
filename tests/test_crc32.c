/* test_crc32.c - unit tests for the CRC-32 checksum.
 * Run with: mingw32-make test */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "crc32.h"

static int failures = 0;
#define CHECK(cond)                                                    \
    do {                                                               \
        if (!(cond)) {                                                 \
            printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);   \
            failures++;                                                \
        }                                                              \
    } while (0)

static uint32_t crc_of(const char *text) {
    return crc32_update(0, (const unsigned char *)text, strlen(text));
}

/* Values that other programs (zlib, gzip, Python) are known to produce. */
static void test_known_values(void) {
    CHECK(crc_of("") == 0x00000000u);
    CHECK(crc_of("a") == 0xE8B7BE43u);
    CHECK(crc_of("abc") == 0x352441C2u);
    CHECK(crc_of("123456789") == 0xCBF43926u);  /* the standard check value */
    CHECK(crc_of("aaaabbc") == 0x9CEEACC2u);    /* the example in docs/FORMAT.md */
    CHECK(crc_of("The quick brown fox jumps over the lazy dog") == 0x414FA339u);
}

static void test_all_byte_values(void) {
    unsigned char all[256];
    for (int i = 0; i < 256; ++i) all[i] = (unsigned char)i;
    CHECK(crc32_update(0, all, 256) == 0x29058C73u);
}

static void test_million_a(void) {
    unsigned char *big = malloc(1000000);
    CHECK(big != NULL);
    if (big == NULL) return;
    memset(big, 'a', 1000000);
    CHECK(crc32_update(0, big, 1000000) == 0xDC25BFBCu);
    free(big);
}

/* Empty input leaves the checksum unchanged, with or without a pointer. */
static void test_empty_input(void) {
    unsigned char dummy = 1;
    CHECK(crc32_update(0, NULL, 0) == 0);
    CHECK(crc32_update(0x12345678u, NULL, 0) == 0x12345678u);
    CHECK(crc32_update(0x12345678u, &dummy, 0) == 0x12345678u);
}

/* Feeding data in two pieces gives the same result as all at once, wherever it is split. */
static void test_incremental(void) {
    unsigned char buf[1000];
    uint32_t seed = 5u;
    for (int i = 0; i < 1000; ++i) {
        seed = seed * 1664525u + 1013904223u;
        buf[i] = (unsigned char)(seed >> 16);
    }
    uint32_t whole = crc32_update(0, buf, 1000);
    int mismatches = 0;
    for (size_t split = 0; split <= 1000; ++split) {
        uint32_t part = crc32_update(0, buf, split);
        part = crc32_update(part, buf + split, 1000 - split);
        if (part != whole) mismatches++;
    }
    CHECK(mismatches == 0);
}

/* A slow, obviously-correct bit-by-bit version to compare against. */
static uint32_t crc_bitwise(const unsigned char *data, size_t size) {
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < size; ++i) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc & 1u) ? (crc >> 1) ^ 0xEDB88320u : crc >> 1;
        }
    }
    return ~crc;
}

static void test_matches_bitwise_version(void) {
    unsigned char buf[300] = {0};
    uint32_t seed = 99u;
    int mismatches = 0;
    for (size_t len = 0; len <= 300; ++len) {
        for (size_t i = 0; i < len; ++i) {
            seed = seed * 1664525u + 1013904223u;
            buf[i] = (unsigned char)(seed >> 16);
        }
        if (crc32_update(0, buf, len) != crc_bitwise(buf, len)) mismatches++;
    }
    CHECK(mismatches == 0);
}

/* A CRC always detects a single flipped bit: this is what the checksum is for. */
static void test_detects_single_bit_flips(void) {
    unsigned char buf[64];
    uint32_t seed = 3u;
    for (int i = 0; i < 64; ++i) {
        seed = seed * 1664525u + 1013904223u;
        buf[i] = (unsigned char)(seed >> 16);
    }
    uint32_t original = crc32_update(0, buf, 64);
    int undetected = 0;
    for (int byte = 0; byte < 64; ++byte) {
        for (int bit = 0; bit < 8; ++bit) {
            buf[byte] ^= (unsigned char)(1u << bit);
            if (crc32_update(0, buf, 64) == original) undetected++;
            buf[byte] ^= (unsigned char)(1u << bit);
        }
    }
    CHECK(undetected == 0);
}

/* Swapping two bytes changes the checksum (a plain sum of the bytes would not notice). */
static void test_detects_swapped_bytes(void) {
    CHECK(crc_of("ab") != crc_of("ba"));
}

int main(void) {
    test_known_values();
    test_all_byte_values();
    test_million_a();
    test_empty_input();
    test_incremental();
    test_matches_bitwise_version();
    test_detects_single_bit_flips();
    test_detects_swapped_bytes();
    if (failures == 0) { printf("crc32: all tests passed\n"); return 0; }
    printf("crc32: %d check(s) FAILED\n", failures);
    return 1;
}