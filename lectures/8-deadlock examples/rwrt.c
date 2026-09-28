#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

int writers; /* Number writer threads that want to enter the
critical section (some or all of these may be blocked)*/
int writing; /* Number of threads that are actually writing inside
the C.S. (c an only be zero or one) */
int reading; /* Number of threads that are actually reading inside
the C.S.
// if writing !=0 then reading must be zero (and vice versa)
*/
pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t turn = PTHREAD_COND_INITIALIZER;
int buf;
void *reader(void *id)
{
    pthread_mutex_lock(&m);
    /*check if any writers waiting*/
    while (writers)
        pthread_cond_wait(&turn, &m);

    /* No need to wait while(writing here) because we can only exit
        the above loop when writing is zero*/
    //while(writing)
    //  pthred_cond_wait(&cv,&m);

    reading++;
    pthread_mutex_unlock(&m);
    /* Read here! */
    printf("thread no: %d, buf: %d \n", (int)id, buf);
    pthread_mutex_lock(&m);
    reading--;
    pthread_cond_signal(&turn);
    pthread_mutex_unlock(&m);
}

void *writer(void *id)
{
    pthread_mutex_lock(&m);
    writers++;
    while (reading || writing)
        pthread_cond_wait(&turn, &m);
    writing++;
   
    // perform writing here
    buf = rand();
   
    writing--;
    writers--;
    pthread_cond_broadcast(&turn);
    pthread_mutex_unlock(&m);
}
int main()
{
    int i;
    pthread_t t[11];

    for (i = 0; i < 10; i++){
        pthread_create(&t[i], NULL, reader, (void *)i);
    
    pthread_create(&t[10], NULL, writer, (void *)(10));
    }
    for (i = 0; i < 11; i++)
        pthread_join(t[i], NULL);
}