/* test_huffman.c - unit tests for huffman_build_tree.
 * Run with: mingw32-make test */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "huffman.h"

static int failures = 0;
#define CHECK(cond)                                                    \
    do {                                                               \
        if (!(cond)) {                                                 \
            printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);   \
            failures++;                                                \
        }                                                              \
    } while (0)

static HuffTree t;

/* The worked example from the explanation: aaaabbc. */
static void test_aaaabbc(void) {
    uint64_t f[256] = {0};
    f['a'] = 4; f['b'] = 2; f['c'] = 1;
    CHECK(huffman_build_tree(f, &t) == 0);
    CHECK(t.root == 257);
    CHECK(t.node_count == 258);
    CHECK(t.nodes[257].freq == 7);
    CHECK(t.nodes[257].left == 256 && t.nodes[257].right == 'a');
    CHECK(t.nodes[256].freq == 3);
    CHECK(t.nodes[256].left == 'c' && t.nodes[256].right == 'b');
    CHECK(t.nodes[257].symbol == -1 && t.nodes['a'].symbol == 'a');
}

/* No bytes at all: an empty tree, and still a success. */
static void test_empty(void) {
    uint64_t f[256] = {0};
    CHECK(huffman_build_tree(f, &t) == 0);
    CHECK(t.root == -1 && t.node_count == 0);
}

/* One distinct byte: a dummy leaf is added so the byte gets a 1-bit code. */
static void test_single_byte(void) {
    uint64_t f[256] = {0};
    f['a'] = 5;
    CHECK(huffman_build_tree(f, &t) == 0);
    CHECK(t.root == 256);
    CHECK(t.nodes[256].freq == 6);                 /* 5 + the dummy's 1 */
    CHECK(t.nodes[256].left == 'b' && t.nodes[256].right == 'a');
    CHECK(f['b'] == 0);                            /* caller's table untouched */
}

/* The dummy for byte 255 wraps around to byte 0. */
static void test_single_byte_255_wraps(void) {
    uint64_t f[256] = {0};
    f[255] = 3;
    CHECK(huffman_build_tree(f, &t) == 0);
    CHECK(t.nodes[t.root].left == 0 && t.nodes[t.root].right == 255);
}

/* All 256 bytes with equal counts: the maximum tree, 511 nodes. */
static void test_all_equal(void) {
    uint64_t f[256];
    for (int i = 0; i < 256; ++i) f[i] = 10;
    CHECK(huffman_build_tree(f, &t) == 0);
    CHECK(t.node_count == 511);
    CHECK(t.nodes[t.root].freq == 2560);
}

/* The root's count equals the total, and building twice gives the same tree. */
static void test_total_and_deterministic(void) {
    uint64_t f[256] = {0}, total = 0;
    HuffTree a, b;
    uint32_t s = 99;
    for (int i = 0; i < 256; ++i) {
        s = s * 1664525u + 1013904223u;
        f[i] = (i % 3 == 0) ? 0 : (s >> 16) % 500;
        total += f[i];
    }
    CHECK(huffman_build_tree(f, &a) == 0);
    CHECK(huffman_build_tree(f, &b) == 0);
    CHECK(a.nodes[a.root].freq == total);
    CHECK(a.root == b.root && a.node_count == b.node_count);
    int same = 1;
    for (int i = 0; i < a.node_count; ++i) {
        if (a.nodes[i].freq != b.nodes[i].freq || a.nodes[i].left != b.nodes[i].left ||
            a.nodes[i].right != b.nodes[i].right || a.nodes[i].symbol != b.nodes[i].symbol) {
            same = 0;
        }
    }
    CHECK(same);
}

static void test_null(void) {
    uint64_t f[256] = {0};
    CHECK(huffman_build_tree(NULL, &t) == -1);
    CHECK(huffman_build_tree(f, NULL) == -1);
}

int main(void) {
    test_aaaabbc();
    test_empty();
    test_single_byte();
    test_single_byte_255_wraps();
    test_all_equal();
    test_total_and_deterministic();
    test_null();
    if (failures == 0) { printf("huffman: all tests passed\n"); return 0; }
    printf("huffman: %d check(s) FAILED\n", failures);
    return 1;
}