#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
int *maddrs[100];
int count = 0;
int used[100];
void *malloc(size_t size)
{
    /* Ask the system for more bytes by extending the heap space.
    sbrk returns -1 on failure
    */
    void *p = sbrk(size);
    if (p == (void *)-1) /* No space left*/
        return NULL;
    maddrs[count++] = p;
    return p;
}

int main()
{
    void *top_of_heap = sbrk(0);
    sbrk(1000);
    malloc(16384); // bazi sistemlerde implementasyonu farkli
    void *top_of_heap2 = sbrk(0);
    printf("The top of heap went from %p to %p \n",
           top_of_heap, top_of_heap2);
}
