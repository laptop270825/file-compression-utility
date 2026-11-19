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

int huffman_encode(const unsigned char *data, size_t size,
                   const HuffCode codes[HUFF_SYMBOLS], BitWriter *bw) {
    if (codes == NULL || bw == NULL || (data == NULL && size>0 )) { /* TODO: the invalid cases: codes is NULL, bw is NULL,
           or data is NULL while size > 0 */
        return -1; // invalid parameters
    }
    for (size_t i = 0; i < size; ++i) {
        HuffCode c = codes[data[i]];
        if (c.length == 0) {
            return -1; // this byte has no code: the table does not match the data
        }
        if (c.length <= 32) {
            int cal = bitwriter_write_bits(bw, c.bits, c.length); // TODO: one call to bitwriter_write_bits with the code and its length.
            if (cal != 0) {
                return -1; // write failure
            }
            // TODO: one call to bitwriter_write_bits with the code and its length.
            //       If it returns non-zero, return -1.
        } else {
            int cal1 = bitwriter_write_bits(bw, c.bits >> 32, c.length - 32); // TODO: two calls. First the leading (c.length - 32) bits, which are c.bits >> 32. Then the low 32 bits, which are c.bits.
            if (cal1 != 0) {
                return -1; // write failure
            }
            int cal2 = bitwriter_write_bits(bw, c.bits, 32); // TODO: two calls. First the leading (c.length - 32) bits, which are c.bits >> 32. Then the low 32 bits, which are c.bits.
            if (cal2 != 0) {
                return -1; //write failure
            // TODO: two calls. First the leading (c.length - 32) bits, which are
            //       c.bits >> 32. Then the low 32 bits, which are c.bits.
            //       If either returns non-zero, return -1.
        }
    }
}
    return 0;
}

int huffman_decode(BitReader *br, const HuffTree *tree, size_t count, unsigned char *out) {
    if (br == NULL || tree == NULL || (out == NULL && count > 0)) {
        return -1; // invalid parameters
    }
    if (count == 0) {
        return 0; // nothing to decode
    }
    if (tree->root < 0 || tree->root >= tree->node_count) {
        return -1; // empty or invalid tree
    }
    if (tree->nodes[tree->root].left == -1 && tree->nodes[tree->root].right == -1) {
        return -1; // a leaf root would decode without reading any bits
    }

    for (size_t i = 0; i < count; ++i) {
        int node = tree->root;
        while (tree->nodes[node].left != -1 && tree->nodes[node].right != -1) {
            uint32_t bit = 0;
            int cal = bitreader_read_bits(br, 1, &bit); // TODO: read ONE bit with bitreader_read_bits(br, 1, &bit).
            if (cal != 0) {
                return -1; // the stream ended too early
            }
            // TODO: read ONE bit with bitreader_read_bits(br, 1, &bit).
            //       If it returns non-zero, return -1 (the stream ended too early).
            if (bit == 1) {
                node = tree->nodes[node].right; // TODO: set node to the right child if bit is 1, otherwise to the left child.
            } else {
                node = tree->nodes[node].left; // TODO: set node to the right child if bit is 1, otherwise to the left child.
            }
            // TODO: set node to the right child if bit is 1, otherwise to the left child.
            if (node < 0 || node >= tree->node_count) {
                return -1; // the tree is malformed
            }
            // TODO: if the new node is outside 0 .. tree->node_count - 1, return -1.
        }
        out[i] = (unsigned char)tree->nodes[node].symbol;
    }
    return 0;
}

int huffman_code_lengths(const HuffCode codes[HUFF_SYMBOLS], unsigned char lengths[HUFF_SYMBOLS]) {
    // TODO: return -1 if codes or lengths is NULL
    if (codes == NULL || lengths == NULL) {
        return -1;
    }
    // TODO: set all 256 lengths to 0 first
    for (int i = 0; i < HUFF_SYMBOLS; ++i) {
        lengths[i] = 0;
    }
    // TODO: for each i, if codes[i].length is < 0 or > 64: set all 256 lengths back to 0 and return -1
    for (int i = 0; i < HUFF_SYMBOLS; ++i) {
        if (codes[i].length < 0 || codes[i].length > 64) {
            for (int j = 0; j < HUFF_SYMBOLS; ++j) {
                lengths[j] = 0;
            }
            return -1;
        }
        //       otherwise lengths[i] = (unsigned char)codes[i].length
        lengths[i] = (unsigned char)codes[i].length;
    }
    return 0;
}

int huffman_canonical_codes(const unsigned char lengths[HUFF_SYMBOLS], HuffCode codes[HUFF_SYMBOLS]) {
    // TODO: NULL check, then reset all 256 codes to bits = 0, length = 0
    if (lengths == NULL || codes == NULL) {
        return -1;
    }
    // Step 1: count how many bytes have each length.
    int count[65] = {0};
    for (int i = 0; i < HUFF_SYMBOLS; ++i) {
        codes[i].bits = 0;
        codes[i].length = 0;
    }
    // TODO: for each byte, if lengths[i] > 64 return -1; else count[lengths[i]]++
    for (int i = 0; i < HUFF_SYMBOLS; ++i) {
        if (lengths[i] > 64) {
            return -1;
        } else {
            count[lengths[i]]++;
        }
    }
    count[0] = 0; // length 0 means "no code": it uses no code space

    // Step 2: validate. `left` = how many codes of the current length are still unused.
    int left = 1;
    for (int len = 1; len <= 64; ++len) {
        // TODO: left = left * 2 - count[len];
        left = left * 2 - count[len];
        // TODO: if left < 0 -> return -1 (over-subscribed)
        if (left < 0) {
            return -1; // over-subscribed
        }
        // TODO: if left > HUFF_SYMBOLS -> return -1 (more free slots than bytes to fill them)
        if (left > HUFF_SYMBOLS) {
            return -1; // more free slots than bytes to fill them
        }
    }
    // TODO: if left != 0 return -1 (incomplete)
    if (left != 0) {
        return -1; // incomplete
    }

    // Step 3: first code of each length.
    uint64_t first_code[65] = {0};
    uint64_t code = 0;
    for (int len = 1; len <= 64; ++len) {
        // TODO: code = (code + (uint64_t)count[len - 1]) << 1;
        code = (code + (uint64_t)count[len - 1]) << 1;
        // TODO: first_code[len] = code;
        first_code[len] = code;
    }

    // Step 4: hand out the codes in byte order 0..255.
    // next_code[len] starts at first_code[len] and goes up by one for every byte of that length.
    uint64_t next_code[65];
    for (int len = 0; len <= 64; ++len) next_code[len] = first_code[len];
    for (int i = 0; i < HUFF_SYMBOLS; ++i) {
        int len = lengths[i];
        if (len > 0) {
            // TODO: codes[i].bits = next_code[len]++;   codes[i].length = len;
            codes[i].bits = next_code[len]++;
            codes[i].length = len;
        }
    }
    return 0;
}

/*static void reset_tree(HuffTree *tree) {
    tree->root = -1;
    tree->node_count = 0;
}

int huffman_tree_from_codes(const HuffCode codes[HUFF_SYMBOLS], HuffTree *tree) {
    // TODO: NULL check; reset_tree(tree)
    if (codes == NULL || tree == NULL) {
        printf("fail at line %d\n", __LINE__);
        goto fail;
    }

    // Leaves: node i is byte i. freq 0, left -1, right -1, symbol i.
    // TODO: loop 0..255 and fill them in
    for (int i = 0; i < HUFF_SYMBOLS; ++i) {
        tree->nodes[i].freq = 0;
        tree->nodes[i].left = -1;
        tree->nodes[i].right = -1;
        tree->nodes[i].symbol = i;
    }

    // TODO: validate every length is 0..64 (else return -1) and count how many bytes have a code (`used`).
    int used = 0;
    for (int i = 0; i < HUFF_SYMBOLS; ++i) {
        if (codes[i].length < 0 || codes[i].length > 64) {
            printf("fail at line %d\n", __LINE__);
            goto fail; // invalid length
        }
        if (codes[i].length > 0) {
            used++;
        }
    }
    // TODO: if used == 0, return 0 (empty table -> empty tree)
    if (used == 0) {
        printf("fail at line %d\n", __LINE__);
        goto fail; // no used symbols
    }

    int next_id = HUFF_SYMBOLS;
    int root = next_id++; // the root is the first internal node
    // TODO: initialise nodes[root]: freq 0, left -1, right -1, symbol -1
    for (int i = 0; i < HUFF_MAX_NODES; ++i) {
        tree->nodes[root].freq = 0;
        tree->nodes[root].left = -1;
        tree->nodes[root].right = -1;
        tree->nodes[root].symbol = -1;
    }

    for (int sym = 0; sym < HUFF_SYMBOLS; ++sym) {
        int len = codes[sym].length;
        if (len == 0) continue;
        // TODO: if len < 64 and (codes[sym].bits >> len) != 0 -> goto fail   (bits don't fit)
        if (len < 64 && (codes[sym].bits >> len) != 0) {
            printf("fail at line %d\n", __LINE__);
            goto fail; // bits don't fit
        }
        int cur = root;
        for (int k = len - 1; k >= 0; --k) {            // from the first bit to the last
            int bit = (int)((codes[sym].bits >> k) & 1u);
            int *slot = bit ? &tree->nodes[cur].right : &tree->nodes[cur].left;
            if (k == 0) {
                // last bit: the slot must be empty, then point it at the leaf `sym`
                // TODO: if (*slot != -1) goto fail;   *slot = sym;
                if (*slot != -1) {
                    printf("fail at line %d\n", __LINE__);
                    goto fail; // slot already occupied
                }
            } else {
                // middle bit: follow the existing node, or create a new internal node
                // TODO: if (*slot == -1): if next_id >= HUFF_MAX_NODES goto fail; make a new internal
                //       node at next_id (freq 0, left -1, right -1, symbol -1); *slot = next_id++;
                if (*slot == -1) {
                    if (next_id >= HUFF_MAX_NODES) {
                        printf("fail at line %d\n", __LINE__);
                        goto fail; // no more room for new nodes
                    }
                    tree->nodes[next_id].freq = 0;
                    tree->nodes[next_id].left = -1;
                    tree->nodes[next_id].right = -1;
                    tree->nodes[next_id].symbol = -1;
                    *slot = next_id++;
                }
                // TODO: else if (*slot < HUFF_SYMBOLS) goto fail;   // we hit a leaf: a shorter code is a prefix of this one
                else if (*slot < HUFF_SYMBOLS) {
                    printf("fail at line %d\n", __LINE__);
                    goto fail; // we hit a leaf: a shorter code is a prefix of this one
                }
                cur = *slot;
            }
        }
    }

    // TODO: every internal node (index 256 .. next_id-1) must have both children, else goto fail
    for (int i = HUFF_SYMBOLS; i < next_id; ++i) {
        if (tree->nodes[i].left == -1 || tree->nodes[i].right == -1) {
            printf("fail at line %d\n", __LINE__);
            goto fail; // internal node missing a child
        }
    }
    tree->root = root;
    tree->node_count = next_id;
    return 0;

fail:
    reset_tree(tree);
    return -1;
}*/

static void reset_tree(HuffTree *tree) {
    tree->root = -1;
    tree->node_count = 0;
}

int huffman_tree_from_codes(const HuffCode codes[HUFF_SYMBOLS], HuffTree *tree) {
    if (codes == NULL || tree == NULL) {
        return -1; // invalid parameters
    }
    reset_tree(tree);

    // The leaves: node i is byte i.
    for (int i = 0; i < HUFF_SYMBOLS; ++i) {
        tree->nodes[i].freq = 0;
        tree->nodes[i].left = -1;
        tree->nodes[i].right = -1;
        tree->nodes[i].symbol = i;
    }

    int used = 0;
    for (int i = 0; i < HUFF_SYMBOLS; ++i) {
        if (codes[i].length < 0 || codes[i].length > 64) {
            return -1;
        }
        if (codes[i].length > 0) {
            used++;
        }
    }
    if (used == 0) {
        return 0; // no codes: an empty tree
    }

    int next_id = HUFF_SYMBOLS;
    int root = next_id++; // the root is the first internal node
    tree->nodes[root].freq = 0;
    tree->nodes[root].left = -1;
    tree->nodes[root].right = -1;
    tree->nodes[root].symbol = -1;

    for (int sym = 0; sym < HUFF_SYMBOLS; ++sym) {
        int len = codes[sym].length;
        if (len == 0) {
            continue;
        }
        if (len < 64 && (codes[sym].bits >> len) != 0) {
            goto fail; // bits do not fit in the length
        }
        int cur = root;
        for (int k = len - 1; k >= 0; --k) {
            int bit = (int)((codes[sym].bits >> k) & 1u);
            int *slot = bit ? &tree->nodes[cur].right : &tree->nodes[cur].left;
            if (k == 0) {
                if (*slot != -1) {
                    goto fail; // this place is already taken: equal codes, or a prefix of another
                }
                *slot = sym;
            } else {
                if (*slot == -1) {
                    if (next_id >= HUFF_MAX_NODES) {
                        goto fail; // too many nodes
                    }
                    tree->nodes[next_id].freq = 0;
                    tree->nodes[next_id].left = -1;
                    tree->nodes[next_id].right = -1;
                    tree->nodes[next_id].symbol = -1;
                    *slot = next_id++;
                } else if (*slot < HUFF_SYMBOLS) {
                    goto fail; // a shorter code ends here: it is a prefix of this one
                }
                cur = *slot;
            }
        }
    }

    // Every internal node must have both children, or a stream could end in a dead end.
    for (int i = HUFF_SYMBOLS; i < next_id; ++i) {
        if (tree->nodes[i].left == -1 || tree->nodes[i].right == -1) {
            goto fail;
        }
    }
    tree->root = root;
    tree->node_count = next_id;
    return 0;

fail:
    reset_tree(tree);
    return -1;
}