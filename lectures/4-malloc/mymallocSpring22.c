/**this code is written in class for educational purposes, and may include a few bugs.
 * This code shows the implementation of a malloc that uses implicit list
 */
#include <stdio.h>
#include <unistd.h>
#define SIZE 1024
typedef struct {
    /* data */
    size_t size;
    int is_free;
    // void *next;
} Info;

typedef struct {
    Info info;
    // Block *next;
    // Block *prev;
    char data[];
} Block;

char *heap_start = NULL;
char *heap_end = NULL;
int size_of_info = 2 * sizeof(Info);

Block *split(Block *b, size_t size);
Block *next_block(Block *b);
Block *prev_block(Block *b);
Block *split(Block *b, size_t size) {
    Block *b1 = b;
    Block *b2 = (Block *)((char *)b + sizeof(Info) + sizeof(Info) + size);

    b2->info.size = b1->info.size - size - sizeof(Info) - sizeof(Info);
    b2->info.is_free = 1;
    b1->info.size = size;
    b1->info.is_free = 0;

    Info *btag1 = (char *)b1 + sizeof(Info) + b1->info.size;
    btag1->is_free = b1->info.is_free;
    btag1->size = b1->info.size;

    Info *btag2 = (char *)b2 + sizeof(Info) + b2->info.size;
    btag2->is_free = 1;
    btag2->size = b2->info.size;

    return b1;
}

Block *next_block(Block *b) {
    Block *next = (Block *)((char *)b + 2 * sizeof(Info) + b->info.size);
    if ((char *)next < heap_end)
        return next;
    return NULL;
}

Block *prev_block(Block *b) {
    Block *prev = (Block *)((char *)b - 2 * sizeof(Info) - b->info.size);
    if ((char *)prev > heap_start)
        return prev;
    return NULL;
}
static int call = 0;

void *mymalloc(size_t size) {
    /*TODO: size = (size + 2*sizeof(Info)+ 15)/16;????****/

    if (call == 0) {
        heap_start = sbrk(SIZE);
        heap_end = heap_start + SIZE;
        Block *b = (Block *)heap_start;
        b->info.size = SIZE - 2 * sizeof(Info);
        b->info.is_free = 1;
        Info *btag = (char *)b + sizeof(Info) + b->info.size;
        btag->is_free = 1;
        btag->size = b->info.size;
        call = 1;
    }
    /*first fit algorithm*/
    Block *b = heap_start;
    while (b != NULL && (b->info.is_free && size > b->info.size)) {
        b = next_block(b);
    }
    if (b != NULL) {
        /*TODO: threshold?*/

        if (size /*+threshold*/ < b->info.size) {
            b = split(b, size);
        }
        b->info.is_free = 0;
        Info *btag = (Info *)((char *)b + b->info.size + sizeof(Info));
        btag->is_free = 0;
        return &b->data;
    }

    return NULL;
}

Block *right_coalescing(Block *b) {
    /*coalesing with the next*/
    Block *next = next_block(b);

    if (next != NULL && next->info.is_free) {
        b->info.is_free = 1;
        b->info.size += next->info.size + 2 * sizeof(Info);
        Info *btag = (char *)b + sizeof(Info) + b->info.size;
        btag->is_free = 1;
        btag->size = b->info.size;
    }

    return b;
}

Block *left_coalescing(Block *b) {
    /*coalesing with the previous*/
    Block *prev = prev_block(b);
    if (prev != NULL && prev->info.is_free) {
        prev->info.size += b->info.size + 2 * sizeof(Info);
        Info *btag = (char *)prev + sizeof(Info) + prev->info.size;
        btag->is_free = 1;
        btag->size = prev->info.size;
        return prev;
    }
    return b;
}
int myfree(void *ptr) {
    if (ptr != NULL) {
        Block *b = (Block *)((char *)ptr - sizeof(Info));
        b->info.is_free = 1;
        Info *btag = (char *)b + sizeof(Info) + b->info.size;
        btag->is_free = 1;
        btag->size = b->info.size;
        b = right_coalescing(b);
        b = left_coalescing(b);
    }
    return 0;
}

int main() {
    int *p1 = mymalloc(sizeof(int) * 10);
    int *p2 = mymalloc(sizeof(int) * 10);
    int *p3 = mymalloc(sizeof(int) * 10);
    myfree(p1);
    myfree(p2);
    myfree(p3);
}