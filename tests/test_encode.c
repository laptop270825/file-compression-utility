/* test_encode.c - unit tests for huffman_encode.
 * Run with: mingw32-make test */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "bitio.h"
#include "huffman.h"

static int failures = 0;
#define CHECK(cond)                                                    \
    do {                                                               \
        if (!(cond)) {                                                 \
            printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);   \
            failures++;                                                \
        }                                                              \
    } while (0)

static HuffTree tree;
static HuffCode codes[HUFF_SYMBOLS];

/* Counts the bytes in data, builds the tree and the code table. */
static int prepare(const unsigned char *data, size_t size) {
    uint64_t f[256] = {0};
    for (size_t i = 0; i < size; ++i) f[data[i]]++;
    if (huffman_build_tree(f, &tree) != 0) return -1;
    return huffman_build_codes(&tree, codes);
}

/* A decoder written just for this test: walk the tree one bit at a time.
 * (The real decoder comes in step 3e.) Returns 0 if `expected` is reproduced exactly. */
static int decode_matches(const unsigned char *bits, size_t nbytes,
                          const unsigned char *expected, size_t count) {
    BitReader br;
    bitreader_init(&br, bits, nbytes);
    for (size_t i = 0; i < count; ++i) {
        int node = tree.root;
        while (tree.nodes[node].left != -1) {
            uint32_t bit = 0;
            if (bitreader_read_bits(&br, 1, &bit) != 0) return -1;
            node = bit ? tree.nodes[node].right : tree.nodes[node].left;
        }
        if (tree.nodes[node].symbol != expected[i]) return -1;
    }
    return 0;
}

/* The worked example: aaaabbc with a=1, b=01, c=00 gives 1111 01 01 00 = F5 00. */
static void test_aaaabbc(void) {
    const unsigned char text[] = "aaaabbc";
    BitWriter bw;
    unsigned char *out = NULL;
    size_t out_size = 0;

    CHECK(prepare(text, 7) == 0);
    CHECK(bitwriter_init(&bw) == 0);
    CHECK(huffman_encode(text, 7, codes, &bw) == 0);
    CHECK(bitwriter_finish(&bw, &out, &out_size) == 0);
    CHECK(out_size == 2);
    if (out != NULL && out_size == 2) {
        CHECK(out[0] == 0xF5);
        CHECK(out[1] == 0x00);
    }
    free(out);
}

/* Nothing to encode: success, and nothing is written. */
static void test_empty(void) {
    BitWriter bw;
    unsigned char *out = NULL;
    size_t out_size = 99;

    CHECK(prepare(NULL, 0) == 0);
    CHECK(bitwriter_init(&bw) == 0);
    CHECK(huffman_encode(NULL, 0, codes, &bw) == 0);
    CHECK(bitwriter_finish(&bw, &out, &out_size) == 0);
    CHECK(out_size == 0);
    free(out);
}

static void test_invalid(void) {
    const unsigned char text[] = "ab";
    BitWriter bw;
    CHECK(prepare(text, 2) == 0);
    CHECK(bitwriter_init(&bw) == 0);
    CHECK(huffman_encode(text, 2, NULL, &bw) == -1);
    CHECK(huffman_encode(text, 2, codes, NULL) == -1);
    CHECK(huffman_encode(NULL, 5, codes, &bw) == -1);
    bitwriter_free(&bw);
}

/* A byte that has no code in the table must be rejected. */
static void test_byte_without_code(void) {
    const unsigned char known[] = "aabb";
    const unsigned char other[] = "abz"; /* 'z' never occurred */
    BitWriter bw;
    CHECK(prepare(known, 4) == 0);
    CHECK(bitwriter_init(&bw) == 0);
    CHECK(huffman_encode(other, 3, codes, &bw) == -1);
    bitwriter_free(&bw);
}

/* A file of one repeated byte: every byte becomes a 1-bit code. */
static void test_single_byte_value(void) {
    unsigned char text[20];
    BitWriter bw;
    unsigned char *out = NULL;
    size_t out_size = 0;
    memset(text, 'a', sizeof text);

    CHECK(prepare(text, sizeof text) == 0);
    CHECK(bitwriter_init(&bw) == 0);
    CHECK(huffman_encode(text, sizeof text, codes, &bw) == 0);
    CHECK(bitwriter_finish(&bw, &out, &out_size) == 0);
    CHECK(out_size == 3); /* 20 bits -> 3 bytes */
    CHECK(out != NULL && decode_matches(out, out_size, text, sizeof text) == 0);
    free(out);
}

static uint32_t lcg_state = 31u;
static uint32_t next_random(void) {
    lcg_state = lcg_state * 1664525u + 1013904223u;
    return lcg_state;
}

/* Random skewed data: encode, then decode with the tree and compare. Also checks the exact size. */
static void test_random_round_trip(void) {
    enum { N = 50000 };
    unsigned char *text = malloc(N);
    CHECK(text != NULL);
    if (text == NULL) return;

    for (int round = 0; round < 6; ++round) {
        uint32_t alphabet = 2u + (next_random() >> 16) % 255; /* 2..256 distinct values */
        for (int i = 0; i < N; ++i) {
            uint32_t r = (next_random() >> 16) % 1000;
            /* skewed: low values far more likely */
            text[i] = (unsigned char)((r * r / 1000 * alphabet) / 1000);
        }
        CHECK(prepare(text, N) == 0);

        uint64_t bits = 0;
        for (int i = 0; i < N; ++i) bits += (uint64_t)codes[text[i]].length;

        BitWriter bw;
        unsigned char *out = NULL;
        size_t out_size = 0;
        CHECK(bitwriter_init(&bw) == 0);
        CHECK(huffman_encode(text, N, codes, &bw) == 0);
        CHECK(bitwriter_finish(&bw, &out, &out_size) == 0);
        CHECK((uint64_t)out_size == (bits + 7) / 8);
        CHECK(out != NULL && decode_matches(out, out_size, text, N) == 0);
        free(out);
    }
    free(text);
}

/* Fibonacci counts force codes longer than 32 bits: they must survive the two-part write. */
static void test_long_codes(void) {
    unsigned char text[4096];
    size_t n = 0;
    uint64_t a = 1, b = 1;
    for (int sym = 0; sym < 40; ++sym) {
        /* byte value `sym` appears fib times, capped so the buffer stays small */
        uint64_t times = a < 60 ? a : 60;
        for (uint64_t k = 0; k < times && n < sizeof text; ++k) text[n++] = (unsigned char)sym;
        uint64_t next = a + b;
        a = b;
        b = next;
    }
    /* The real counts must be Fibonacci for a deep tree, so count them separately. */
    uint64_t f[256] = {0};
    a = 1; b = 1;
    for (int sym = 0; sym < 40; ++sym) {
        f[sym] = a;
        uint64_t next = a + b;
        a = b;
        b = next;
    }
    CHECK(huffman_build_tree(f, &tree) == 0);
    CHECK(huffman_build_codes(&tree, codes) == 0);
    int max_len = 0;
    for (int i = 0; i < 256; ++i) if (codes[i].length > max_len) max_len = codes[i].length;
    CHECK(max_len > 32);

    BitWriter bw;
    unsigned char *out = NULL;
    size_t out_size = 0;
    CHECK(bitwriter_init(&bw) == 0);
    CHECK(huffman_encode(text, n, codes, &bw) == 0);
    CHECK(bitwriter_finish(&bw, &out, &out_size) == 0);
    CHECK(out != NULL && decode_matches(out, out_size, text, n) == 0);
    free(out);
}

int main(void) {
    test_aaaabbc();
    test_empty();
    test_invalid();
    test_byte_without_code();
    test_single_byte_value();
    test_random_round_trip();
    test_long_codes();
    if (failures == 0) { printf("encode: all tests passed\n"); return 0; }
    printf("encode: %d check(s) FAILED\n", failures);
    return 1;
}