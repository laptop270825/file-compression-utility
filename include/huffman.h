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

/*
 * huffman_code_lengths - copy the length of every code into a plain byte array.
 *
 * codes   : a code table (from huffman_build_codes or huffman_canonical_codes).
 * lengths : output, lengths[b] is the code length of byte b (0 = no code).
 *           This is the 256-byte table that is stored in the compressed file.
 *
 * Returns 0 on success. Returns -1 for a NULL argument or a length outside
 * 0..64; lengths is then all zeros.
 */
int huffman_code_lengths(const HuffCode codes[HUFF_SYMBOLS], unsigned char lengths[HUFF_SYMBOLS]);

/*
 * huffman_canonical_codes - turn code lengths into the canonical codes.
 *
 * lengths : lengths[b] is the code length of byte b, 0 meaning "no code".
 *           Untrusted input (it comes from a file), so it is fully validated.
 * codes   : output table. Codes are handed out in order of increasing length,
 *           and for equal lengths in increasing byte value; each code is the
 *           previous one plus 1, shifted left by one bit when the length grows.
 *
 * Returns 0 on success. Returns -1 for a NULL argument, a length above 64, or
 * lengths that are not a complete prefix code (over-subscribed or with unused
 * code space). That includes all zeros and a single byte with length 1.
 * On failure every entry of codes is reset to length 0.
 */
int huffman_canonical_codes(const unsigned char lengths[HUFF_SYMBOLS], HuffCode codes[HUFF_SYMBOLS]);

/*
 * huffman_tree_from_codes - build a decoding tree from a code table.
 *
 * codes : a code table, for example from huffman_canonical_codes.
 * tree  : output, in the same layout as huffman_build_tree: leaf b is node b,
 *         internal nodes start at index 256. Works with huffman_decode.
 *
 * Returns 0 on success. Returns -1 for a NULL argument, a length above 64, bits
 * that do not fit in the length, two codes where one is the start of the other
 * (or equal), or a code that leaves a branch unfinished.
 * An all-zero table gives an empty tree (root = -1) and success.
 * On failure the tree is reset to empty.
 */
int huffman_tree_from_codes(const HuffCode codes[HUFF_SYMBOLS], HuffTree *tree);

#endif /* CMPZ_HUFFMAN_H */