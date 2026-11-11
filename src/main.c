#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>
#include "fileio.h"
#include "freq.h"

int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <file>\n", argv[0]);
        return 1;
    }

    unsigned char *data = NULL;
    size_t size = 0;
    if (read_file(argv[1], &data, &size) == -1) {
        fprintf(stderr, "Error: could not read file '%s'\n", argv[1]);
        return 1;
    }

    uint64_t freq[256];
    if (count_frequencies(data, size, freq) == -1) {
        fprintf(stderr, "Error: could not count frequencies\n");
        free(data);
        return 1;
    }

    printf("File size: %llu bytes\n", (unsigned long long)size);
    for (int b = 0; b < 256; b++) {
        if (freq[b] > 0) {
            printf("Byte %3d '%c': %llu\n", b, isprint(b) ? b : '.',
                   (unsigned long long)freq[b]);
        }
    }

    free(data);
    return 0;
}