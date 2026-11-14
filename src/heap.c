#include <stdlib.h>
#include <heap.h>

static int item_less(HeapItem a, HeapItem b) {
    if ((a.freq < b.freq) || (a.freq == b.freq && a.id < b.id)) {
        return 1;
    }
    return 0;
}

static void swap_items(HeapItem *a, HeapItem *b) {
    HeapItem temp = *a;
    *a = *b;
    *b = temp;
}

static void sift_up(MinHeap *h, size_t i) {
    while (i > 0) {
        size_t parent = (i - 1) / 2;
        if (item_less(h->items[i], h->items[parent])) {
            swap_items(&h->items[i], &h->items[parent]);
            i = parent;
        }
        else if(i==0) {
            break;
        }
        else {
            break;
        }
    }
}

static void sift_down(MinHeap *h, size_t i) {
    size_t left, right, smallest;
    while (left < h->size || right < h->size) {
        left = 2 * i + 1;
        right = 2 * i + 2;
        smallest = i;

        if (left < h->size && item_less(h->items[left], h->items[smallest])) {
            smallest = left;
        }
        if (right < h->size && item_less(h->items[right], h->items[smallest])) {
            smallest = right;
        }
        if (smallest != i) {
            swap_items(&h->items[i], &h->items[smallest]);
            i = smallest;
        } else {
            break;
        }
    }
}

int heap_init(MinHeap *h, size_t capacity) {
    if (h == NULL || capacity == 0) {
        return -1; // invalid parameters
    }
    h->items = malloc(capacity * sizeof(HeapItem));
    if (!h->items) {
        h->size = 0;
        h->capacity = 0;
        return -1; // memory allocation failed, heap is safely empty
    }
    h->size = 0;
    h->capacity = capacity;
    return 0; // success
}

int heap_push(MinHeap *h, HeapItem item) {
    if (!h || h->size >= h->capacity) {
        return -1; // invalid parameters, or the heap is full
    }
    h->items[h->size] = item;
    h->size++;
    sift_up(h, h->size - 1);
    return 0; // success
}

int heap_pop(MinHeap *h, HeapItem *out) {
    if (!h || h->size == 0) {
        return -1; // invalid parameters or heap is empty
    }
    *out = h->items[0];
    h->items[0] = h->items[h->size - 1];
    h->size--;
    sift_down(h, 0);
    return 0; // success
}

void heap_free(MinHeap *h) {
    if (!h || !h->items) {
        return; // nothing to free
    }
    free(h->items);
    h->items = NULL;
    h->size = 0;
    h->capacity = 0;
}