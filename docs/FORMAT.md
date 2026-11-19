# CMPZ file format, version 1

CMPZ is a lossless compressed file format. It stores one file as: a fixed header,
a table of Huffman code lengths, and the Huffman-coded data. This document is the
authoritative description. Any program that follows it can read and write CMPZ files.

## Conventions

- **Integers** are stored **little-endian** (least significant byte first), written
  byte by byte. Structures are never copied to disk directly, so a file is identical
  on every platform.
- **Bits** in the compressed data are packed **most significant bit first**: the first
  bit of the stream is the highest bit (bit 7) of the first byte.
- A byte is 8 bits. "Size" always means a number of bytes.

## File layout

| Offset | Size | Field | Value |
|-------:|-----:|-------|-------|
| 0 | 4 | magic | ASCII `CMPZ` (`43 4D 50 5A`) |
| 4 | 1 | version | `1` |
| 5 | 1 | flags | bit 0 = STORED; all other bits must be 0 |
| 6 | 2 | reserved | must be `00 00` |
| 8 | 8 | original size | size of the uncompressed data (unsigned) |
| 16 | 4 | CRC-32 | CRC-32 of the uncompressed data |
| 20 | ... | body | depends on the STORED flag |

The header is always 20 bytes.

### Body, STORED flag set (flags = 1)

The original bytes follow the header unchanged. The file size must be exactly
`20 + original size`. The writer uses this mode for empty files and whenever
compressing would not make the file smaller.

### Body, STORED flag clear (flags = 0)

| Offset | Size | Field |
|-------:|-----:|-------|
| 20 | 256 | code length table: one byte per byte value 0..255 |
| 276 | ... | payload: the Huffman-coded data, then padding |

**Code length table.** Entry `i` is the length in bits (1..64) of the code for byte
value `i`, or `0` if the byte value does not occur in the data.

**Payload.** For each original byte, in order, the bits of its code are written,
most significant bit first. After the last code, the remaining bits of the final
byte (0 to 7 bits) are padding and must be zero. Exactly `original size` codes are
decoded; the padding is never interpreted as data.

## Canonical code assignment

Only the lengths are stored. The codes are rebuilt from them by this fixed rule, so
a writer and a reader always agree:

```
count[L]  = number of byte values whose length is L        (count[0] = 0)
code      = 0
for L = 1 to 64:
    code          = (code + count[L-1]) << 1
    first_code[L] = code
for byte value b = 0 to 255, in increasing order, with length L > 0:
    code(b) = first_code[L] + (number of smaller byte values with length L)
```

The code of a byte is written with exactly `length` bits (leading zeros included).

### Example

Lengths: `a` = 1, `b` = 2, `c` = 2. Then `count[1] = 1`, `count[2] = 2`,
`first_code[1] = 0`, `first_code[2] = (0 + 1) << 1 = 2`. The codes are:

| Byte | Length | Code |
|------|-------:|------|
| `a` | 1 | `0` |
| `b` | 2 | `10` |
| `c` | 2 | `11` |

The text `aaaabbc` becomes the bits `0 0 0 0 10 10 11` = `0000101011`, which is
10 bits, padded to the two bytes `0A C0`.

If this text were stored as a compressed file, the header would be
`43 4D 50 5A 01 00 00 00 07 00 00 00 00 00 00 00 C2 AC EE 9C`
(original size 7, CRC-32 `0x9CEEACC2`). In practice a file this small is written
in STORED mode, because the 256-byte table would make it larger.

## Rules a reader must enforce

A reader must reject the file (and must not crash or read out of bounds) if:

1. it is shorter than 20 bytes, or the magic or version does not match;
2. any reserved flag bit or reserved byte is not zero;
3. (STORED) the file size is not `20 + original size`;
4. (compressed) the file is shorter than 276 bytes;
5. (compressed) a code length is greater than 64;
6. (compressed) the code lengths do not form a **complete prefix code**: at least two
   byte values must have a length, and the sum of `2^-length` over all of them must be
   exactly 1. (An over-subscribed or incomplete table cannot be decoded safely.);
7. (compressed) `original size` is zero, or larger than `8 * payload size` (every
   byte needs at least one bit, so a larger value cannot be true; this stops a tiny
   file from demanding a huge allocation);
8. (compressed) the bits run out before `original size` bytes are decoded;
9. (compressed) after decoding, there is more than one byte of data left over, or
   any padding bit is not zero;
10. the CRC-32 of the decoded data does not equal the stored CRC-32.
11. Reject any flag bits other than the defined ones.

## Writer rules

- Use the STORED mode when `original size` is 0, or when the compressed form
  (276 bytes plus the payload) would not be smaller than `20 + original size`.
  The file can therefore never grow by more than 20 bytes.
- The same input always produces exactly the same output file (deterministic).

## Limits of the reference implementation

The whole file is held in memory, so it is meant for files up to a few hundred
megabytes. Code lengths up to 64 bits are supported.

## Versioning

The version byte lets future formats be told apart. A reader that sees a version it
does not know must reject the file instead of guessing.