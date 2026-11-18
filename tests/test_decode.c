/* test_decode.c - unit tests for huffman_decode.
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

/* Encodes with the real encoder; the caller frees *out. */
static int encode(const unsigned char *data, size_t size, unsigned char **out, size_t *out_size) {
    BitWriter bw;
    if (bitwriter_init(&bw) != 0) return -1;
    if (huffman_encode(data, size, codes, &bw) != 0) {
        bitwriter_free(&bw);
        return -1;
    }
    return bitwriter_finish(&bw, out, out_size);
}

/* The worked example in reverse: F5 00 decodes back to aaaabbc. */
static void test_aaaabbc(void) {
    const unsigned char text[] = "aaaabbc";
    const unsigned char bits[2] = {0xF5, 0x00};
    unsigned char out[7];
    BitReader br;

    CHECK(prepare(text, 7) == 0);
    bitreader_init(&br, bits, 2);
    CHECK(huffman_decode(&br, &tree, 7, out) == 0);
    CHECK(memcmp(out, "aaaabbc", 7) == 0);
}

/* Decoding zero symbols always succeeds and reads nothing, even with an empty tree. */
static void test_count_zero(void) {
    uint64_t f[256] = {0};
    BitReader br;
    unsigned char dummy = 0;
    CHECK(huffman_build_tree(f, &tree) == 0);
    bitreader_init(&br, NULL, 0);
    CHECK(huffman_decode(&br, &tree, 0, NULL) == 0);
    CHECK(huffman_decode(&br, &tree, 0, &dummy) == 0);
    CHECK(br.bit_pos == 0);
}

/* An empty tree cannot decode anything. */
static void test_empty_tree_with_data(void) {
    uint64_t f[256] = {0};
    const unsigned char bits[1] = {0xFF};
    unsigned char out[4];
    BitReader br;
    CHECK(huffman_build_tree(f, &tree) == 0);
    bitreader_init(&br, bits, 1);
    CHECK(huffman_decode(&br, &tree, 4, out) == -1);
}

static void test_invalid_arguments(void) {
    const unsigned char text[] = "aabbc";
    const unsigned char bits[2] = {0xF5, 0x00};
    unsigned char out[7];
    BitReader br;

    CHECK(prepare(text, 5) == 0);
    bitreader_init(&br, bits, 2);
    CHECK(huffman_decode(NULL, &tree, 3, out) == -1);
    CHECK(huffman_decode(&br, NULL, 3, out) == -1);
    CHECK(huffman_decode(&br, &tree, 3, NULL) == -1);
}

/* A stream that ends before `count` symbols are decoded is an error, not a crash. */
static void test_truncated_stream(void) {
    const unsigned char text[] = "aaaabbc";
    const unsigned char bits[1] = {0xF5}; /* only 8 of the 10 bits */
    unsigned char out[7];
    BitReader br;

    CHECK(prepare(text, 7) == 0);
    bitreader_init(&br, bits, 1);
    CHECK(huffman_decode(&br, &tree, 7, out) == -1);

    bitreader_init(&br, NULL, 0);
    CHECK(huffman_decode(&br, &tree, 1, out) == -1);
}

/* Damaged trees must be refused instead of followed into invalid memory. */
static void test_corrupt_tree(void) {
    const unsigned char text[] = "aaaabbc";
    const unsigned char bits[2] = {0xF5, 0x00};
    unsigned char out[7];
    BitReader br;
    HuffTree bad;

    CHECK(prepare(text, 7) == 0);

    bad = tree;
    bad.nodes[bad.root].right = 5000; /* child index far outside the array */
    bitreader_init(&br, bits, 2);
    CHECK(huffman_decode(&br, &bad, 7, out) == -1);

    bad = tree;
    bad.nodes[bad.root].left = -7; /* an impossible child index */
    bitreader_init(&br, bits, 2);
    CHECK(huffman_decode(&br, &bad, 7, out) == -1);

    bad = tree;
    bad.root = 'a'; /* a leaf as the root */
    bitreader_init(&br, bits, 2);
    CHECK(huffman_decode(&br, &bad, 7, out) == -1);

    bad = tree;
    bad.root = 9999; /* root outside the tree */
    bitreader_init(&br, bits, 2);
    CHECK(huffman_decode(&br, &bad, 7, out) == -1);
}

static uint32_t lcg_state = 77u;
static uint32_t next_random(void) {
    lcg_state = lcg_state * 1664525u + 1013904223u;
    return lcg_state;
}

/* Encode with the real encoder, decode with the real decoder, expect the original back. */
static void round_trip(const unsigned char *text, size_t n) {
    unsigned char *packed = NULL, *back = malloc(n > 0 ? n : 1);
    size_t packed_size = 0;
    BitReader br;

    CHECK(back != NULL);
    CHECK(prepare(text, n) == 0);
    CHECK(encode(text, n, &packed, &packed_size) == 0);
    bitreader_init(&br, packed, packed_size);
    CHECK(huffman_decode(&br, &tree, n, back) == 0);
    CHECK(n == 0 || memcmp(back, text, n) == 0);
    free(packed);
    free(back);
}

static void test_round_trips(void) {
    enum { N = 40000 };
    unsigned char *text = malloc(N);
    unsigned char one[1] = {42};
    unsigned char same[1000];
    CHECK(text != NULL);
    if (text == NULL) return;

    round_trip(NULL, 0);                 /* empty file */
    round_trip(one, 1);                  /* a single byte */
    memset(same, 'x', sizeof same);
    round_trip(same, sizeof same);       /* one value repeated */

    for (int i = 0; i < N; ++i) text[i] = (unsigned char)(next_random() >> 16); /* uniform: all 256 values */
    round_trip(text, N);

    for (int round = 0; round < 8; ++round) {
        uint32_t alphabet = 2u + (next_random() >> 16) % 255;
        for (int i = 0; i < N; ++i) {
            uint32_t r = (next_random() >> 16) % 1000;
            text[i] = (unsigned char)((r * r / 1000 * alphabet) / 1000); /* skewed */
        }
        round_trip(text, N);
    }
    free(text);
}

/* Fibonacci-shaped data gives codes longer than 32 bits; decoding must still match. */
static void test_long_codes_round_trip(void) {
    unsigned char text[4096];
    size_t n = 0;
    uint64_t a = 1, b = 1;
    uint64_t f[256] = {0};
    unsigned char *packed = NULL, back[4096];
    size_t packed_size = 0;
    BitReader br;

    for (int sym = 0; sym < 40; ++sym) {
        uint64_t times = a < 60 ? a : 60;
        for (uint64_t k = 0; k < times && n < sizeof text; ++k) text[n++] = (unsigned char)sym;
        f[sym] = a;
        uint64_t next = a + b;
        a = b;
        b = next;
    }
    CHECK(huffman_build_tree(f, &tree) == 0);
    CHECK(huffman_build_codes(&tree, codes) == 0);
    CHECK(encode(text, n, &packed, &packed_size) == 0);
    bitreader_init(&br, packed, packed_size);
    CHECK(huffman_decode(&br, &tree, n, back) == 0);
    CHECK(memcmp(back, text, n) == 0);
    free(packed);
}

/* Random garbage as input: the result may be success or -1, but never a crash. */
static void test_garbage_input(void) {
    enum { SIZE = 2000, COUNT = 6000 };
    unsigned char text[] = "the quick brown fox jumps over the lazy dog";
    unsigned char garbage[SIZE], out[COUNT];
    BitReader br;
    int results_ok = 1;

    CHECK(prepare(text, sizeof text - 1) == 0);
    for (int round = 0; round < 200; ++round) {
        for (int i = 0; i < SIZE; ++i) garbage[i] = (unsigned char)(next_random() >> 16);
        size_t len = (size_t)(next_random() >> 16) % (SIZE + 1);
        bitreader_init(&br, garbage, len);
        int r = huffman_decode(&br, &tree, COUNT, out);
        if (r != 0 && r != -1) results_ok = 0;
    }
    CHECK(results_ok);
}

int main(void) {
    test_aaaabbc();
    test_count_zero();
    test_empty_tree_with_data();
    test_invalid_arguments();
    test_truncated_stream();
    test_corrupt_tree();
    test_round_trips();
    test_long_codes_round_trip();
    test_garbage_input();
    if (failures == 0) { printf("decode: all tests passed\n"); return 0; }
    printf("decode: %d check(s) FAILED\n", failures);
    return 1;
}