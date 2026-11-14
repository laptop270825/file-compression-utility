/* test_heap.c - unit tests for the min-heap.
 * Run with: mingw32-make test */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "heap.h"

static int failures = 0;

#define CHECK(cond)                                                    \
    do {                                                               \
        if (!(cond)) {                                                 \
            printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);   \
            failures++;                                                \
        }                                                              \
    } while (0)

static HeapItem item(uint64_t freq, int id) {
    HeapItem it;
    it.freq = freq;
    it.id = id;
    return it;
}

/* Setting up the heap with bad arguments must fail cleanly. */
static void test_init_invalid(void) {
    MinHeap h;
    CHECK(heap_init(NULL, 4) == -1);
    CHECK(heap_init(&h, 0) == -1);
}

/* Popping from an empty heap is an error, not a crash. */
static void test_pop_empty(void) {
    MinHeap h;
    HeapItem out;
    CHECK(heap_init(&h, 4) == 0);
    CHECK(heap_pop(&h, &out) == -1);
    heap_free(&h);
}

/* The worked example: push 5, 3, 8, 1 and pop them in sorted order. */
static void test_sorted_pop(void) {
    MinHeap h;
    HeapItem out;
    CHECK(heap_init(&h, 8) == 0);
    CHECK(heap_push(&h, item(5, 0)) == 0);
    CHECK(heap_push(&h, item(3, 1)) == 0);
    CHECK(heap_push(&h, item(8, 2)) == 0);
    CHECK(heap_push(&h, item(1, 3)) == 0);
    CHECK(h.size == 4);
    CHECK(heap_pop(&h, &out) == 0 && out.freq == 1);
    CHECK(heap_pop(&h, &out) == 0 && out.freq == 3);
    CHECK(heap_pop(&h, &out) == 0 && out.freq == 5);
    CHECK(heap_pop(&h, &out) == 0 && out.freq == 8);
    CHECK(heap_pop(&h, &out) == -1);
    heap_free(&h);
}

/* Equal counts are ordered by id: this is the deterministic tie-break. */
static void test_tie_break(void) {
    MinHeap h;
    HeapItem out;
    CHECK(heap_init(&h, 8) == 0);
    CHECK(heap_push(&h, item(5, 30)) == 0);
    CHECK(heap_push(&h, item(5, 10)) == 0);
    CHECK(heap_push(&h, item(5, 20)) == 0);
    CHECK(heap_push(&h, item(2, 99)) == 0);
    CHECK(heap_pop(&h, &out) == 0 && out.freq == 2 && out.id == 99);
    CHECK(heap_pop(&h, &out) == 0 && out.freq == 5 && out.id == 10);
    CHECK(heap_pop(&h, &out) == 0 && out.freq == 5 && out.id == 20);
    CHECK(heap_pop(&h, &out) == 0 && out.freq == 5 && out.id == 30);
    heap_free(&h);
}

/* A full heap refuses more items, and keeps working afterwards. */
static void test_full(void) {
    MinHeap h;
    HeapItem out;
    CHECK(heap_init(&h, 3) == 0);
    CHECK(heap_push(&h, item(3, 0)) == 0);
    CHECK(heap_push(&h, item(2, 1)) == 0);
    CHECK(heap_push(&h, item(1, 2)) == 0);
    CHECK(heap_push(&h, item(0, 3)) == -1);
    CHECK(h.size == 3);
    CHECK(heap_pop(&h, &out) == 0 && out.freq == 1);
    CHECK(heap_push(&h, item(0, 3)) == 0);
    CHECK(heap_pop(&h, &out) == 0 && out.freq == 0);
    heap_free(&h);
}

/* Freeing twice, or freeing NULL, must be harmless. */
static void test_free_twice(void) {
    MinHeap h;
    CHECK(heap_init(&h, 4) == 0);
    heap_free(&h);
    heap_free(&h);
    heap_free(NULL);
}

static uint32_t lcg_state = 2024u;
static uint32_t next_random(void) {
    lcg_state = lcg_state * 1664525u + 1013904223u;
    return lcg_state;
}

/* Push 1000 random items (many duplicate counts) and check they come out sorted. */
static void test_random_sorted(void) {
    enum { COUNT = 1000 };
    MinHeap h;
    HeapItem prev = item(0, -1);
    int ok_order = 1;
    int popped = 0;
    HeapItem out;

    CHECK(heap_init(&h, COUNT) == 0);
    for (int i = 0; i < COUNT; ++i) {
        CHECK(heap_push(&h, item((next_random() >> 16) % 50, i)) == 0);
    }
    while (heap_pop(&h, &out) == 0) {
        if (popped > 0) {
            int in_order = prev.freq < out.freq || (prev.freq == out.freq && prev.id < out.id);
            if (!in_order) {
                ok_order = 0;
            }
        }
        prev = out;
        popped++;
    }
    CHECK(ok_order);
    CHECK(popped == COUNT);
    heap_free(&h);
}

/* The way Huffman will use it: take the two smallest, push their sum, repeat.
 * The last item left must carry the total of all the original counts. */
static void test_huffman_style_merge(void) {
    MinHeap h;
    HeapItem a, b, last;
    uint64_t total = 0;
    int next_id = 256;

    CHECK(heap_init(&h, 256) == 0);
    for (int i = 0; i < 256; ++i) {
        uint64_t f = 1 + (next_random() >> 16) % 1000;
        total += f;
        CHECK(heap_push(&h, item(f, i)) == 0);
    }
    while (h.size > 1) {
        CHECK(heap_pop(&h, &a) == 0);
        CHECK(heap_pop(&h, &b) == 0);
        CHECK(heap_push(&h, item(a.freq + b.freq, next_id++)) == 0);
    }
    CHECK(h.size == 1);
    CHECK(heap_pop(&h, &last) == 0);
    CHECK(last.freq == total);
    CHECK(next_id == 256 + 255); /* 256 leaves need exactly 255 merges */
    heap_free(&h);
}

int main(void) {
    test_init_invalid();
    test_pop_empty();
    test_sorted_pop();
    test_tie_break();
    test_full();
    test_free_twice();
    test_random_sorted();
    test_huffman_style_merge();

    if (failures == 0) {
        printf("heap: all tests passed\n");
        return 0;
    }
    printf("heap: %d check(s) FAILED\n", failures);
    return 1;
}