#ifndef CMPZ_HEAP_H
#define CMPZ_HEAP_H
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint64_t freq;   /* how many times the symbol(s) appear */
    int id;          /* which tree node this item stands for */
} HeapItem;

typedef struct {
    HeapItem *items; /* the array that holds the heap */
    size_t size;     /* how many items are in the heap now */
    size_t capacity; /* the most items it can hold */
} MinHeap;

//heap_init allocates room for capacity items. It returns -1 if the allocation fails, or if capacity is 0
int  heap_init(MinHeap *h, size_t capacity);

//heap_push adds an item to the heap. It returns -1 if the heap is full, or 0 on success
int  heap_push(MinHeap *h, HeapItem item);

//heap_pop removes the minimum item from the heap and stores it in *out. It returns -1 if the heap is empty, or 0 on success
int  heap_pop(MinHeap *h, HeapItem *out);

//heap_free frees the memory allocated for the heap. It does not free the MinHeap struct itself, only the internal array.
//and is safe to call twice on the same heap (the second call will do nothing)
void heap_free(MinHeap *h);

#endif // CMPZ_HEAP_H