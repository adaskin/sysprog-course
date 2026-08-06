#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
static int avail = 0;
void* producer(void * p){
    int s = pthread_mutex_lock(&mtx); 
    if (s != 0)
        perror("pthread_mutex_lock");
    
    avail++; /* Let consumer know another unit is available */
    pthread_cond_signal(&cond);
    s = pthread_mutex_unlock(&mtx); 
    if (s != 0) perror("pthread_mutex_unlock");
}

int main(){
    for (;;) {
        int s = pthread_mutex_lock(&mtx); 
        while(avail == 0) 
            pthread_cond_wait(&cond, &mtx);
            /*note: as part of the call to pthread_cond_wait(), 
            the thread releases its lock on the mutex.*/

        if (s != 0) 
            perror("pthread_mutex_lock");
        while (avail > 0) { /* Consume all available units */ 
        /* Do something with produced unit */
            avail--;
        }
        s = pthread_mutex_unlock(&mtx); 
        if (s != 0) 
            perror("pthread_mutex_unlock");
}
