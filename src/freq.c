#include <string.h>
#include "freq.h"

int count_frequencies(const unsigned char *data, size_t size, uint64_t freq[256]) {
    
    if (freq == NULL) {
        return -1; // Error: freq array is NULL
    }

    if (data == NULL && size > 0) {
        return -1; // Error: there are bytes to count but data is NULL
    }

    memset(freq, 0, 256 * sizeof(freq[0])); // Initialize frequency array to zero//Reset all 256 counters to zero
    
    for (size_t i = 0; i < size; ++i) {
        freq[data[i]]++;
    }
    return 0; // Success
}