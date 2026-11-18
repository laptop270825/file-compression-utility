#ifndef CMPZ_HUFFMAN_H
#define CMPZ_HUFFMAN_H

#include <stddef.h>
#include <stdint.h>

#include "bitio.h"

#define HUFF_SYMBOLS   256
#define HUFF_MAX_NODES (2 * HUFF_SYMBOLS - 1)   /* 511 */

/* It builds the tree from the counts in freq, which it does not modify.*/
/* It returns 0 on success, or -1 on bad arguments or failure. */
/* It modifies the tree parameter to contain the constructed Huffman tree. */
/* If all counts are zero, the  tree is empty(root = -1,node_count = 0) */
/* If exactly one byte has a non-zero count, a dummy leaf is added(so decoders must use the same function)*/
/* Ties between nodes of equal frequency are broken by selecting the node with the smaller index */

typedef struct {
    uint64_t freq;   /* total count under this node */
    int left;        /* index of left child, or -1 for a leaf */
    int right;       /* index of right child, or -1 for a leaf */
    int symbol;      /* the byte value for a leaf, or -1 for an internal node */
} HuffNode;

typedef struct {
    HuffNode nodes[HUFF_MAX_NODES];
    int node_count;  /* how many nodes are in use */
    int root;        /* index of the root, or -1 if the tree is empty */
} HuffTree;

int huffman_build_tree(const uint64_t freq[HUFF_SYMBOLS], HuffTree *tree);

/* It builds the Huffman codes from the constructed tree. */
/* The table is filled with zeroes first, that unused bytes get length 0 */
/* It returns 0 on success, or -1 on bad arguments or failure. */
/* It modifies the codes parameter to contain the constructed Huffman codes. */
/* On failure, every entry is reset to length 0*/
typedef struct {
    uint64_t bits;   /* the code, in the lowest `length` bits; its first bit is the highest of those */
    int length;      /* number of bits in the code; 0 means this byte has no code */
} HuffCode;

int huffman_build_codes(const HuffTree *tree, HuffCode codes[HUFF_SYMBOLS]);

/*
 * huffman_encode - write the code of every input byte into a bit stream.
 *
 * data  : the bytes to encode. May be NULL only when size is 0.
 * size  : number of bytes in data.
 * codes : the code table from huffman_build_codes.
 * bw    : an initialised BitWriter that receives the bits. This function does
 *         NOT call bitwriter_finish; the caller does that when all data is written.
 *
 * Returns 0 on success. Returns -1 for a NULL argument, a write failure, or a
 * byte whose code has length 0 (the table does not match the data).
 * Codes longer than 32 bits are written in two parts, because
 * bitwriter_write_bits accepts at most 32 bits per call.
 */
int huffman_encode(const unsigned char *data, size_t size,
                   const HuffCode codes[HUFF_SYMBOLS], BitWriter *bw);

/*
 * huffman_decode - read `count` symbols from a bit stream and rebuild the bytes.
 *
 * br    : a BitReader positioned at the first code. It advances past the codes read.
 * tree  : the same tree the encoder used (rebuilt from the same counts).
 * count : how many bytes to decode. The decoder stops after exactly this many,
 *         so the zero padding in the last byte is never mistaken for data.
 * out   : receives the bytes; the caller provides room for `count` bytes.
 *         May be NULL only when count is 0.
 *
 * Returns 0 on success. Returns -1 for a NULL argument, an empty or malformed
 * tree (while count > 0), or a stream that ends before `count` symbols are decoded.
 * It never reads or writes out of bounds, even for corrupt input.
 */
int huffman_decode(BitReader *br, const HuffTree *tree, size_t count, unsigned char *out);

#endif /* CMPZ_HUFFMAN_H */