/*queue (ring buffer) with semaphores taken from the book*/
#include <pthread.h>
#include <semaphore.h>
#define N (16) // N must be 2^i
void *b[N];
int in = 0, out = 0;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
sem_t countsem, spacesem;
void init()
{
    sem_init(&countsem, 0, 0);
    sem_init(&spacesem, 0, 16);
}

void *dequeue()
{
    // Wait if there are no items in the buffer
    sem_wait(&countsem);
    pthread_mutex_lock(&lock);
    void *result = b[(out++) & (N - 1)];
    pthread_mutex_unlock(&lock);
    // Increment the count of the number of spaces
    sem_post(&spacesem);
    return result;
}

void enqueue(void *value)
{
    // wait if there is no space left:
    sem_wait(&spacesem);
    pthread_mutex_lock(&lock);
    b[(in++) & (N - 1)] = value;
    pthread_mutex_unlock(&lock);
    // increment the count of the number of items
    sem_post(&countsem);
}

int main()
{
    /*exercise: waht happens when you change the order of lock and sems*/
    /*create two threads: one reads items from stdin and the other prints them*/
}