#include "huffman.h"
#include "heap.h"

int huffman_build_tree(const uint64_t freq[HUFF_SYMBOLS], HuffTree *tree) {
    if (freq == NULL || tree == NULL) {
        return -1; // invalid parameters
    }

    // Step 1: make one leaf for each of the 256 possible byte values.
    int used = 0;       // how many different bytes actually occur
    int last_used = -1; // the last byte value that occurs
    for (int i = 0; i < HUFF_SYMBOLS; ++i) {
        tree->nodes[i].freq = freq[i];
        tree->nodes[i].left = -1;
        tree->nodes[i].right = -1;
        tree->nodes[i].symbol = i;
        if (freq[i] > 0) {
            used++;
            last_used = i;
        }
    }
    tree->root = -1;
    tree->node_count = 0;

    // Step 2: handle the special cases.
    if (used == 0) {
        return 0; // empty input: an empty tree is a valid result
    }
    if (used == 1) {
        // A lone byte would get a 0-bit code. Add a dummy leaf so it gets 1 bit.
        int dummy = (last_used + 1) % HUFF_SYMBOLS;
        tree->nodes[dummy].freq = 1;
    }

    // Step 3: put every leaf that occurs into the min-heap.
    MinHeap heap;
    if (heap_init(&heap, HUFF_SYMBOLS) != 0) {
        return -1; // memory allocation failed
    }

    int result = -1;                // stays -1 unless everything succeeds
    int next_id = HUFF_SYMBOLS;     // index for the next internal node
    HeapItem top;

    for (int i = 0; i < HUFF_SYMBOLS; ++i) {
        if (tree->nodes[i].freq > 0) {
            HeapItem item;
            item.freq = tree->nodes[i].freq;
            item.id = i;
            if (heap_push(&heap, item) != 0) {
                goto done;
            }
        }
    }

    // Step 4: repeatedly join the two smallest items under a new node.
    while (heap.size > 1) {
        HeapItem x, y, merged;
        if (heap_pop(&heap, &x) != 0 || heap_pop(&heap, &y) != 0) {
            goto done;
        }
        HuffNode *node = &tree->nodes[next_id];
        node->freq = x.freq + y.freq;
        node->left = x.id;   // the smaller one goes left
        node->right = y.id;
        node->symbol = -1;

        merged.freq = node->freq;
        merged.id = next_id;
        if (heap_push(&heap, merged) != 0) {
            goto done;
        }
        next_id++;
    }

    // Step 5: the one item left is the root.
    if (heap_pop(&heap, &top) != 0) {
        goto done;
    }
    tree->root = top.id;
    tree->node_count = next_id;
    result = 0;

done:
    heap_free(&heap); // runs on every path after heap_init succeeded
    return result;
}

static int assign_codes(const HuffTree *tree, int node, uint64_t bits, int length, HuffCode codes[HUFF_SYMBOLS]) {
    if (node < 0 || node >= tree->node_count) {
        return -1; // reached a null node, nothing to do
    }
    const HuffNode *n = &tree->nodes[node];
    if (n->left == -1 && n->right == -1) {
        // Leaf node: assign the code
        codes[n->symbol].bits = bits;
        codes[n->symbol].length = length;
        return 0;
    }
    if (length >= 64) {
        return -1; // code length exceeds 64 bits, which is not supported
    }
    // Internal node: traverse left and right
    if (assign_codes(tree, n->left, (bits << 1), length + 1, codes) != 0) {
        return -1; // propagate error
    }
    if (assign_codes(tree, n->right, (bits << 1) | 1, length + 1, codes) != 0) {
        return -1; // propagate error
    }
    return 0;
}

int huffman_build_codes(const HuffTree *tree, HuffCode codes[HUFF_SYMBOLS]) {
    if (tree == NULL || codes == NULL) {
        return -1; // invalid parameters
    }
    // Initialize all codes to zero length
    for (int i = 0; i < HUFF_SYMBOLS; ++i) {
        codes[i].bits = 0;
        codes[i].length = 0;
    }
    if (tree->root == -1) {
        return 0; // empty tree, all codes remain zero length
    }
    if (tree->root < 0 || tree->root >= tree->node_count) {
        return -1; // invalid root index
    }
    if (tree->nodes[tree->root].left == -1 && tree->nodes[tree->root].right == -1) {
        return -1; // invalid tree structure, root cannot be a leaf
    }
    if (assign_codes(tree, tree->root, 0, 0, codes) != 0) {
        for (int i = 0; i < HUFF_SYMBOLS; ++i) { // never leave a half-built table
            codes[i].bits = 0;
            codes[i].length = 0;
        }
        return -1;
    }
    return 0;
}