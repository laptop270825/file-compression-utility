/* test_codes.c - unit tests for huffman_build_codes.
 * Run with: mingw32-make test */
#include <stdio.h>
#include <stdint.h>
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

/* The worked example: a = 1, c = 00, b = 01. */
static void test_aaaabbc(void) {
    uint64_t f[256] = {0};
    f['a'] = 4; f['b'] = 2; f['c'] = 1;
    CHECK(huffman_build_tree(f, &tree) == 0);
    CHECK(huffman_build_codes(&tree, codes) == 0);
    CHECK(codes['a'].length == 1 && codes['a'].bits == 1);
    CHECK(codes['b'].length == 2 && codes['b'].bits == 1);  /* 01 */
    CHECK(codes['c'].length == 2 && codes['c'].bits == 0);  /* 00 */
    int others = 0;
    for (int i = 0; i < 256; ++i) {
        if (i != 'a' && i != 'b' && i != 'c' && codes[i].length != 0) others++;
    }
    CHECK(others == 0);
}

/* An empty tree gives an all-zero table. */
static void test_empty(void) {
    uint64_t f[256] = {0};
    CHECK(huffman_build_tree(f, &tree) == 0);
    codes[5].length = 7; /* stale data that must be cleared */
    CHECK(huffman_build_codes(&tree, codes) == 0);
    int nonzero = 0;
    for (int i = 0; i < 256; ++i) nonzero += codes[i].length != 0;
    CHECK(nonzero == 0);
}

/* One distinct byte: both it and its dummy get 1-bit codes. */
static void test_single_byte(void) {
    uint64_t f[256] = {0};
    f['a'] = 5;
    CHECK(huffman_build_tree(f, &tree) == 0);
    CHECK(huffman_build_codes(&tree, codes) == 0);
    CHECK(codes['b'].length == 1 && codes['b'].bits == 0);  /* the dummy */
    CHECK(codes['a'].length == 1 && codes['a'].bits == 1);
}

static void test_invalid(void) {
    uint64_t f[256] = {0};
    HuffTree bad;
    f['a'] = 4; f['b'] = 2;
    CHECK(huffman_build_tree(f, &tree) == 0);
    CHECK(huffman_build_codes(NULL, codes) == -1);
    CHECK(huffman_build_codes(&tree, NULL) == -1);

    bad = tree;
    bad.root = 999; /* outside the tree */
    CHECK(huffman_build_codes(&bad, codes) == -1);

    bad = tree;
    bad.root = 'a'; /* a leaf cannot be the root */
    CHECK(huffman_build_codes(&bad, codes) == -1);
}

/* Prefix-free, complete, and optimal: checks on many different count tables. */
static void check_code_properties(const uint64_t f[256]) {
    uint64_t total = 0, cost = 0, internal_sum = 0;
    int used = 0, max_len = 0;

    CHECK(huffman_build_tree(f, &tree) == 0);
    CHECK(huffman_build_codes(&tree, codes) == 0);

    for (int i = 0; i < 256; ++i) {
        if (codes[i].length > 0) {
            used++;
            if (codes[i].length > max_len) max_len = codes[i].length;
        }
        cost += f[i] * (uint64_t)codes[i].length;
        total += f[i];
    }
    CHECK(used >= 2);
    CHECK(max_len <= 62);

    /* 1. No code is the beginning of another code. */
    int prefix_clashes = 0;
    for (int i = 0; i < 256; ++i) {
        for (int j = 0; j < 256; ++j) {
            if (i == j || codes[i].length == 0 || codes[j].length == 0) continue;
            if (codes[i].length <= codes[j].length &&
                (codes[j].bits >> (codes[j].length - codes[i].length)) == codes[i].bits) {
                prefix_clashes++;
            }
        }
    }
    CHECK(prefix_clashes == 0);

    /* 2. The code is complete: the sum of 2^-length is exactly 1. */
    uint64_t kraft = 0;
    for (int i = 0; i < 256; ++i) {
        if (codes[i].length > 0) kraft += (uint64_t)1 << (max_len - codes[i].length);
    }
    CHECK(kraft == ((uint64_t)1 << max_len));

    /* 3. Total encoded bits equals the sum of all internal node counts (the weighted path length). */
    for (int k = HUFF_SYMBOLS; k < tree.node_count; ++k) internal_sum += tree.nodes[k].freq;
    CHECK(cost == internal_sum);

    /* 4. Never worse than plain 8-bit bytes (a dummy leaf adds at most its own 1 bit). */
    CHECK(cost <= 8 * total + 1);

    /* 5. Walking the tree with each code's bits (first bit = highest) must end exactly at
     *    that byte's leaf. This is what the decoder will do. */
    int walk_errors = 0;
    for (int i = 0; i < 256; ++i) {
        if (codes[i].length == 0) continue;
        int node = tree.root;
        for (int k = 0; k < codes[i].length && node >= 0; ++k) {
            unsigned bit = (unsigned)(((uint64_t)codes[i].bits >> (codes[i].length - 1 - k)) & 1u);
            node = bit ? tree.nodes[node].right : tree.nodes[node].left;
        }
        if (node < 0 || tree.nodes[node].left != -1 || tree.nodes[node].symbol != i) walk_errors++;
    }
    CHECK(walk_errors == 0);
}

static uint32_t lcg_state = 7u;
static uint32_t next_random(void) {
    lcg_state = lcg_state * 1664525u + 1013904223u;
    return lcg_state;
}

static void test_random_tables(void) {
    for (int round = 0; round < 40; ++round) {
        uint64_t f[256];
        uint32_t range = 1u + (next_random() >> 16) % 2000;
        uint32_t density = 2u + (next_random() >> 16) % 10; /* 1 in `density` bytes is absent */
        int present = 0;
        for (int i = 0; i < 256; ++i) {
            f[i] = (next_random() >> 16) % density == 0 ? 0 : 1 + (next_random() >> 16) % range;
            present += f[i] > 0;
        }
        if (present < 2) { f[3] = 5; f[200] = 9; }
        check_code_properties(f);
    }
}

/* Fibonacci counts make the deepest possible tree: codes longer than 32 bits. */
static void fibonacci_counts(uint64_t f[256], int n) {
    uint64_t a = 1, b = 1;
    for (int i = 0; i < 256; ++i) f[i] = 0;
    for (int i = 0; i < n; ++i) {
        f[i] = a;
        uint64_t next = a + b;
        a = b;
        b = next;
    }
}

static void test_long_codes(void) {
    uint64_t f[256];
    fibonacci_counts(f, 40);
    check_code_properties(f);
    int max_len = 0;
    for (int i = 0; i < 256; ++i) if (codes[i].length > max_len) max_len = codes[i].length;
    CHECK(max_len == 39); /* longer than 32 bits, so a 32-bit table would break */
}

/* A code of exactly 64 bits still works; 65 bits is refused and the table is cleared. */
static void test_length_limit(void) {
    uint64_t f[256];
    int max_len = 0;

    fibonacci_counts(f, 65);
    CHECK(huffman_build_tree(f, &tree) == 0);
    CHECK(huffman_build_codes(&tree, codes) == 0);
    for (int i = 0; i < 256; ++i) if (codes[i].length > max_len) max_len = codes[i].length;
    CHECK(max_len == 64);

    fibonacci_counts(f, 66);
    CHECK(huffman_build_tree(f, &tree) == 0);
    CHECK(huffman_build_codes(&tree, codes) == -1);
    int nonzero = 0;
    for (int i = 0; i < 256; ++i) nonzero += codes[i].length != 0;
    CHECK(nonzero == 0);
}

int main(void) {
    test_aaaabbc();
    test_empty();
    test_single_byte();
    test_invalid();
    test_random_tables();
    test_long_codes();
    test_length_limit();
    if (failures == 0) { printf("codes: all tests passed\n"); return 0; }
    printf("codes: %d check(s) FAILED\n", failures);
    return 1;
}