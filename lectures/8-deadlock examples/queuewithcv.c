/*
a queue (ring buffer) with condition variables taken from
https://docs.oracle.com/cd/E19455-01/806-5257/6je9h032r/index.html#sync-ex-33
*/
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#define BSIZE 16
typedef struct
{
    char buf[BSIZE];
    int occupied;          /*buffer busy                   */
    int in;                /*index of next in              */
    int out;               /*index of next out             */
    pthread_mutex_t mutex; /*to lock the buffer and cvs    */
    pthread_cond_t more;   /*there is item in the buffer   */
    pthread_cond_t less;   /*there is room in the buffer   */
} buffer_t;

buffer_t buffer;
char dequeue(buffer_t *b)
{
    char item;
    pthread_mutex_lock(&b->mutex);
    while (b->occupied <= 0)
    {
        pthread_cond_wait(&b->more, &b->mutex);
        /*note: as part of the call to pthread_cond_wait(), 
        the thread releases its lock on the mutex.*/
    }
    assert(b->occupied > 0);

    item = b->buf[b->out++];
    b->out &= BSIZE - 1;
    b->occupied--;

    /* now: either b->occupied > 0 and b->out is the index
       of the next occupied slot in the buffer, or
       b->occupied == 0 and b->out is the index of the next
       (empty) slot that will be filled by a producer (such as
       b->out == b->nin) */

    pthread_cond_signal(&b->less);
    pthread_mutex_unlock(&b->mutex);

    return (item);
}

void enqueue(buffer_t *b, char item)
{
    pthread_mutex_lock(&b->mutex);

    while (b->occupied >= BSIZE)
        pthread_cond_wait(&b->less, &b->mutex);

    assert(b->occupied < BSIZE);

    b->buf[b->in++] = item;

    b->in &= BSIZE - 1;
    b->occupied++;

    /* now: either b->occupied < BSIZE and b->in is the index
       of the next empty slot in the buffer, or
       b->occupied == BSIZE and b->in is the index of the
       next (occupied) slot that will be emptied by a consumer
       (such as b->in == b->out) */

    pthread_cond_signal(&b->more);
    pthread_mutex_unlock(&b->mutex);
}

int main()
{
    /* exercise: create 2 threads: consumer calls dequeue and  prints the item
   producer calls enqueue and reads an item from the stdin
   */
}
