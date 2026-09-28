/*this code is written in class for educational purpose
to particularly explain **pointer arithmetics** in malloc based on implicit list.
The code is not completed, may include a few bugs,
 and may be written in a more modular way.
adaskin, March 2024
*/

#include <inttypes.h>
#include <unistd.h>
#define HEAP_SIZE 1024

static char *heap_start = NULL;
static char *heap_end = NULL;
static char *last_block = NULL;

typedef struct tag {
    uint64_t data_size; /*8 byte*/
    uint32_t isfree; /*1: free, 0:not free, 4byte*/
    uint32_t padding; /*4byte padding, total size 16 byte*/
} Tag;

typedef struct block {
    Tag tag;
    char data[];

} Block;

enum strategy { FIRST_FIT, NEXT_FIT, BEST_FIT, WORST_FIT };

/*|meta.........btag|meta   btag|meta   btag|  |*/
Block *create_block(void *start_addr, size_t size) {
    /*|.........|*/
    int data_size = size - sizeof(Block) - sizeof(Tag);
    if (data_size < 0) return NULL;
    Block *block = (Block *)start_addr;
    block->tag.isfree = 1;
    block->tag.data_size = data_size;
    Tag *btag = (char *)start_addr + sizeof(Block) + data_size;
    btag->data_size = data_size;
    btag->isfree = 1;
    return block;
}

/*|meta.........btag|meta   btag|meta   btag|  |*/
Block *find_free_block(size_t data_size, enum strategy strategy) {
    if (strategy == FIRST_FIT) {
        Block *b = heap_start;
        while ((char *)b < heap_end &&
               (b->tag.isfree == 0 && b->tag.data_size < data_size)) {
            b = (char *)b + sizeof(Block) + sizeof(b->tag.data_size) +
                sizeof(Tag);
        }
        if ((char *)b < heap_start) {
            last_block = (char *)b;
            return (Block *)b;
        }
    } else if (strategy == NEXT_FIT) {
        /*circular array-|heap_start ...........| | |
         * last_block|.....heap_end|*/
        Block *b = last_block;
        while ((char *)b < heap_end &&
               (b->tag.isfree == 0 && b->tag.data_size < data_size)) {
            b = (Block *)((char *)b + sizeof(Block) + sizeof(b->tag.data_size) +
                          sizeof(Tag));
        }
        if ((char *)b < heap_start) {
            last_block = (char *)b;
            return b;
        }
        b = (Block *)heap_start;
        while ((char *)b < last_block &&
               (b->tag.isfree == 0 && b->tag.data_size < data_size)) {
            b = (Block *)((char *)b + sizeof(Block) + sizeof(b->tag.data_size) +
                          sizeof(Tag));
        }
        if ((char *)b < heap_start) {
            last_block = (char *)b;
            return b;
        }
    }

    return NULL;
}

/*use 16 byte blocks*/
size_t align_size(size_t size) {
    int n16 = (size + sizeof(Block) + sizeof(Tag) + 15) / 16;
    size_t new_size = size * n16;
    return new_size;
}

/* |tag..........................tag| */
Block *split_block(Block *b, size_t data_size) {
    Block *left = NULL, *right = NULL;
    size_t left_size = data_size;
    left = b;
    right = left + sizeof(Block) + sizeof(Tag) + data_size;
    /* |lefttag.........tag|rigthtag...........tag| */
    right->tag.data_size =
        b->tag.data_size - left_size - sizeof(Tag) - sizeof(Block);
    right->tag.isfree = 1;
    Tag *tag = (Tag *)((char *)right + right->tag.data_size);
    tag->isfree = 1;
    tag->data_size = right->tag.data_size;
    
    /*same for left, you may write a function e.g. set_tags*/
    left->tag.data_size = left_size;
    left->tag.isfree = 1;
    tag = (Tag *)((char *)left + left->tag.data_size);
    tag->isfree = 1;
    tag->data_size = left->tag.data_size;
    return left;
}
void *mymalloc(size_t size) {
    static int first = 1;
    if (first == 1) {
        heap_start = sbrk(HEAP_SIZE);
        if (heap_start == (char *)-1) {
            perror("sbrk error: not available memory");
            return NULL;
        }
        heap_end = heap_start + HEAP_SIZE;
        Block *b = create_block(heap_start, HEAP_SIZE);
        first = 0;
    }

    size = align_size(size);
    Block *b = find_free_block(size, FIRST_FIT);

    if (b != NULL && size < b->tag.data_size) {
        Block *b2 = split_block(b, size);
        b2->tag.isfree = 0; /*not free*/
        return b2->data;
    }

    return NULL;
}

void myfree(void *p) {
    Block *b = (Block *)((char *)p - sizeof(Block));
    b->tag.isfree = 1;
    Tag *endtag = (Tag *)((char *)p + b->tag.data_size);
    endtag->isfree = 1;

    /*eger linked list olsaydi
        insert_to_free_list(b)
    */
    /*coalescing*/
    /*|left..........tag|b.....tag|*/
    Tag *lefttag = (Tag *)((char *)b + sizeof(Tag *));
    if (lefttag->isfree == 1) {
        Block *left = lefttag->data_size + sizeof(Block);
        left->tag.data_size += b->tag.data_size + sizeof(Tag) + sizeof(Block);
        Tag *newlefttag = (char *)left + left->tag.data_size;
        newlefttag->data_size = left->tag.data_size;
        newlefttag->isfree = 1;
    }
    Block *right = next_block(b);
    if (right != NULL && right->tag.isfree == 1) {
        /*right coalescing*/
    }
}
/*you can use this function to make loops 
in mymalloc and myfree more concise*/
Block *next_block(Block *b) {
    Block *right =
        (Block *)((char *)b + sizeof(Block) + b->tag.data_size + sizeof(Tag));
    if ((char *)right < heap_end) {
        return right;
    }
    return NULL;
}

/*you can use this function to make loops
 in mymalloc and myfree more concise*/
Block *prev_block(Block *b) {
    /*similar to next block*/
    return NULL;
}
int main() {
    int *a = mymalloc(14);
    int *b = mymalloc(14);
    a[1] = 14;
}