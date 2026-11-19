/* test_canonical.c - unit tests for canonical Huffman codes.
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

static uint32_t lcg_state = 2024u;
static uint32_t next_random(void) {
    lcg_state = lcg_state * 1664525u + 1013904223u;
    return lcg_state;
}

/* true when no code is equal to, or the beginning of, another code */
static int is_prefix_free(const HuffCode codes[HUFF_SYMBOLS]) {
    for (int a = 0; a < HUFF_SYMBOLS; ++a) {
        if (codes[a].length == 0) continue;
        for (int b = 0; b < HUFF_SYMBOLS; ++b) {
            if (a == b || codes[b].length == 0 || codes[b].length < codes[a].length) continue;
            /* b is at least as long as a: does b start with a? */
            uint64_t start = codes[b].bits >> (codes[b].length - codes[a].length);
            if (start == codes[a].bits) return 0;
        }
    }
    return 1;
}

/* The worked example from docs/FORMAT.md: a=1, b=2, c=2 gives 0, 10, 11. */
static void test_worked_example(void) {
    unsigned char lengths[256] = {0};
    HuffCode codes[256];
    lengths['a'] = 1;
    lengths['b'] = 2;
    lengths['c'] = 2;
    CHECK(huffman_canonical_codes(lengths, codes) == 0);
    CHECK(codes['a'].length == 1 && codes['a'].bits == 0x0);
    CHECK(codes['b'].length == 2 && codes['b'].bits == 0x2);
    CHECK(codes['c'].length == 2 && codes['c'].bits == 0x3);
    CHECK(codes['d'].length == 0);
}

/* "aaaabbc" with canonical codes is 0A C0, and the decoder reads it back. */
static void test_aaaabbc_payload(void) {
    const unsigned char text[] = "aaaabbc";
    uint64_t f[256] = {0};
    HuffTree tree, tree2;
    HuffCode codes[256], canon[256];
    unsigned char lengths[256], *packed = NULL, back[7];
    size_t packed_size = 0;
    BitWriter bw;
    BitReader br;

    for (int i = 0; i < 7; ++i) f[text[i]]++;
    CHECK(huffman_build_tree(f, &tree) == 0);
    CHECK(huffman_build_codes(&tree, codes) == 0);
    CHECK(huffman_code_lengths(codes, lengths) == 0);
    CHECK(huffman_canonical_codes(lengths, canon) == 0);

    CHECK(bitwriter_init(&bw) == 0);
    CHECK(huffman_encode(text, 7, canon, &bw) == 0);
    CHECK(bitwriter_finish(&bw, &packed, &packed_size) == 0);
    CHECK(packed_size == 2);
    CHECK(packed != NULL && packed[0] == 0x0A && packed[1] == 0xC0);

    CHECK(huffman_tree_from_codes(canon, &tree2) == 0);
    bitreader_init(&br, packed, packed_size);
    CHECK(huffman_decode(&br, &tree2, 7, back) == 0);
    CHECK(memcmp(back, "aaaabbc", 7) == 0);
    free(packed);
}

static void test_code_lengths(void) {
    HuffCode codes[256];
    unsigned char lengths[256];
    memset(codes, 0, sizeof codes);
    codes[5].length = 3;
    codes[200].length = 64;
    CHECK(huffman_code_lengths(codes, lengths) == 0);
    CHECK(lengths[5] == 3 && lengths[200] == 64 && lengths[0] == 0);

    codes[7].length = 65;
    CHECK(huffman_code_lengths(codes, lengths) == -1);
    CHECK(lengths[5] == 0 && lengths[200] == 0); /* reset on failure */
    codes[7].length = -1;
    CHECK(huffman_code_lengths(codes, lengths) == -1);

    CHECK(huffman_code_lengths(NULL, lengths) == -1);
    CHECK(huffman_code_lengths(codes, NULL) == -1);
}

/* Full pipeline on data: tree lengths -> canonical codes -> encode -> rebuild tree -> decode. */
static void pipeline(const unsigned char *text, size_t n) {
    uint64_t f[256] = {0};
    HuffTree tree, tree2;
    HuffCode codes[256], canon[256];
    unsigned char lengths[256], *packed = NULL, *back = malloc(n > 0 ? n : 1);
    size_t packed_size = 0;
    BitWriter bw;
    BitReader br;

    CHECK(back != NULL);
    for (size_t i = 0; i < n; ++i) f[text[i]]++;
    CHECK(huffman_build_tree(f, &tree) == 0);
    CHECK(huffman_build_codes(&tree, codes) == 0);
    CHECK(huffman_code_lengths(codes, lengths) == 0);
    CHECK(huffman_canonical_codes(lengths, canon) == 0);

    /* same lengths as the tree, and a valid prefix code */
    for (int i = 0; i < 256; ++i) CHECK(canon[i].length == codes[i].length);
    CHECK(is_prefix_free(canon));

    CHECK(bitwriter_init(&bw) == 0);
    CHECK(huffman_encode(text, n, canon, &bw) == 0);
    CHECK(bitwriter_finish(&bw, &packed, &packed_size) == 0);

    CHECK(huffman_tree_from_codes(canon, &tree2) == 0);
    bitreader_init(&br, packed, packed_size);
    CHECK(huffman_decode(&br, &tree2, n, back) == 0);
    CHECK(n == 0 || memcmp(back, text, n) == 0);
    free(packed);
    free(back);
}

static void test_pipeline(void) {
    enum { N = 30000 };
    unsigned char *text = malloc(N);
    unsigned char one[1] = {9};
    unsigned char same[500];
    CHECK(text != NULL);
    if (text == NULL) return;

    pipeline(one, 1);                    /* one byte value: the dummy leaf gives it 1 bit */
    memset(same, 'z', sizeof same);
    pipeline(same, sizeof same);
    for (int i = 0; i < N; ++i) text[i] = (unsigned char)(next_random() >> 16);
    pipeline(text, N);                   /* all 256 values */
    for (int round = 0; round < 10; ++round) {
        uint32_t alphabet = 2u + (next_random() >> 16) % 255;
        for (int i = 0; i < N; ++i) {
            uint32_t r = (next_random() >> 16) % 1000;
            text[i] = (unsigned char)((r * r / 1000 * alphabet) / 1000);
        }
        pipeline(text, N);
    }
    free(text);
}

/* Fibonacci-shaped counts give codes of about 40 bits. */
static void test_long_codes(void) {
    unsigned char text[4096];
    size_t n = 0;
    uint64_t a = 1, b = 1;
    for (int sym = 0; sym < 40; ++sym) {
        uint64_t times = a < 60 ? a : 60;
        for (uint64_t k = 0; k < times && n < sizeof text; ++k) text[n++] = (unsigned char)sym;
        uint64_t next = a + b;
        a = b;
        b = next;
    }
    pipeline(text, n);
}

/* Anything that is not a complete prefix code must be refused, and the table reset. */
static void test_invalid_lengths(void) {
    unsigned char lengths[256];
    HuffCode codes[256];

    memset(lengths, 0, sizeof lengths);                    /* all zeros */
    CHECK(huffman_canonical_codes(lengths, codes) == -1);

    lengths['a'] = 1;                                      /* a single byte: incomplete */
    CHECK(huffman_canonical_codes(lengths, codes) == -1);

    lengths['b'] = 2;                                      /* 0, 10 and then 11 unused */
    CHECK(huffman_canonical_codes(lengths, codes) == -1);
    CHECK(codes['a'].length == 0 && codes['b'].length == 0);

    memset(lengths, 0, sizeof lengths);                    /* three codes of length 1 */
    lengths['a'] = lengths['b'] = lengths['c'] = 1;
    CHECK(huffman_canonical_codes(lengths, codes) == -1);

    memset(lengths, 0, sizeof lengths);                    /* length above 64 */
    lengths['a'] = 65;
    lengths['b'] = 1;
    CHECK(huffman_canonical_codes(lengths, codes) == -1);

    memset(lengths, 0, sizeof lengths);                    /* length 255 */
    lengths['a'] = 255;
    CHECK(huffman_canonical_codes(lengths, codes) == -1);

    CHECK(huffman_canonical_codes(NULL, codes) == -1);
    CHECK(huffman_canonical_codes(lengths, NULL) == -1);
}

/* Valid extremes: two codes of length 1, and all 256 bytes with length 8. */
static void test_valid_extremes(void) {
    unsigned char lengths[256];
    HuffCode codes[256];

    memset(lengths, 0, sizeof lengths);
    lengths[0] = 1;
    lengths[255] = 1;
    CHECK(huffman_canonical_codes(lengths, codes) == 0);
    CHECK(codes[0].bits == 0 && codes[255].bits == 1);

    memset(lengths, 8, sizeof lengths);
    CHECK(huffman_canonical_codes(lengths, codes) == 0);
    for (int i = 0; i < 256; ++i) CHECK(codes[i].length == 8 && codes[i].bits == (uint64_t)i);
}

static void test_tree_from_bad_codes(void) {
    HuffCode codes[256];
    HuffTree tree;

    memset(codes, 0, sizeof codes);
    CHECK(huffman_tree_from_codes(codes, &tree) == 0);     /* empty table, empty tree */
    CHECK(tree.root == -1);

    codes['a'].bits = 0; codes['a'].length = 1;            /* 0 and 01: a is a prefix of b */
    codes['b'].bits = 1; codes['b'].length = 2;
    CHECK(huffman_tree_from_codes(codes, &tree) == -1);
    CHECK(tree.root == -1 && tree.node_count == 0);

    memset(codes, 0, sizeof codes);                        /* 01 and 0: the short code comes later in byte order */
    codes['a'].bits = 1; codes['a'].length = 2;
    codes['b'].bits = 0; codes['b'].length = 1;
    CHECK(huffman_tree_from_codes(codes, &tree) == -1);

    memset(codes, 0, sizeof codes);                        /* two equal codes */
    codes['a'].bits = 0; codes['a'].length = 1;
    codes['b'].bits = 0; codes['b'].length = 1;
    CHECK(huffman_tree_from_codes(codes, &tree) == -1);

    memset(codes, 0, sizeof codes);                        /* incomplete: only 0 and 10 */
    codes['a'].bits = 0; codes['a'].length = 1;
    codes['b'].bits = 2; codes['b'].length = 2;
    CHECK(huffman_tree_from_codes(codes, &tree) == -1);

    /* Otherwise-complete tables that break the prefix rule: the tree must still be refused. */
    memset(codes, 0, sizeof codes);                        /* 0, 01, 10, 11: "0" is the start of "01" */
    codes['a'].bits = 0; codes['a'].length = 1;
    codes['b'].bits = 2; codes['b'].length = 2;
    codes['c'].bits = 3; codes['c'].length = 2;
    codes['d'].bits = 1; codes['d'].length = 2;
    CHECK(huffman_tree_from_codes(codes, &tree) == -1);

    memset(codes, 0, sizeof codes);                        /* 0, 0, 10, 11: the code "0" is used twice */
    codes['a'].bits = 0; codes['a'].length = 1;
    codes['b'].bits = 0; codes['b'].length = 1;
    codes['c'].bits = 2; codes['c'].length = 2;
    codes['d'].bits = 3; codes['d'].length = 2;
    CHECK(huffman_tree_from_codes(codes, &tree) == -1);

    memset(codes, 0, sizeof codes);                        /* bits do not fit in length */
    codes['a'].bits = 4; codes['a'].length = 1;
    codes['b'].bits = 1; codes['b'].length = 1;
    CHECK(huffman_tree_from_codes(codes, &tree) == -1);

    memset(codes, 0, sizeof codes);                        /* bad length */
    codes['a'].bits = 0; codes['a'].length = 70;
    CHECK(huffman_tree_from_codes(codes, &tree) == -1);
    codes['a'].length = -3;
    CHECK(huffman_tree_from_codes(codes, &tree) == -1);

    CHECK(huffman_tree_from_codes(NULL, &tree) == -1);
    CHECK(huffman_tree_from_codes(codes, NULL) == -1);
}

/* Random garbage lengths: never a crash, and anything accepted must be a real prefix code. */
static void test_garbage_lengths(void) {
    unsigned char lengths[256];
    HuffCode codes[256];
    HuffTree tree;
    int accepted_ok = 1, accepted = 0;

    for (int round = 0; round < 3000; ++round) {
        memset(lengths, 0, sizeof lengths);
        int used = 2 + (int)(next_random() >> 16) % 6;
        int maxlen = 1 + (int)(next_random() >> 16) % 5;
        for (int k = 0; k < used; ++k) {
            lengths[(next_random() >> 16) % 256] = (unsigned char)(1 + (next_random() >> 16) % (uint32_t)maxlen);
        }
        if (round % 7 == 0) lengths[(next_random() >> 16) % 256] = (unsigned char)(next_random() >> 16); /* maybe > 64 */
        if (huffman_canonical_codes(lengths, codes) == 0) {
            accepted++;
            if (!is_prefix_free(codes)) accepted_ok = 0;
            if (huffman_tree_from_codes(codes, &tree) != 0) accepted_ok = 0;
        }
    }
    CHECK(accepted_ok);
    CHECK(accepted > 0); /* the generator does produce some valid ones */
}

int main(void) {
    test_worked_example();
    test_aaaabbc_payload();
    test_code_lengths();
    test_pipeline();
    test_long_codes();
    test_invalid_lengths();
    test_valid_extremes();
    test_tree_from_bad_codes();
    test_garbage_lengths();
    if (failures == 0) { printf("canonical: all tests passed\n"); return 0; }
    printf("canonical: %d check(s) FAILED\n", failures);
    return 1;
}