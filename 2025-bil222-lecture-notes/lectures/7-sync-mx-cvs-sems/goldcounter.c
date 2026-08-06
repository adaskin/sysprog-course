#include <stdio.h>
#include <semaphore.h>
#include <pthread.h>
#define NTHREADS 10
#define NGOLD 100000000
int sum = 0; /*shared memory*/

pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;

pthread_mutex_t m2 = PTHREAD_MUTEX_INITIALIZER;
sem_t s;

void *countgold(void *arg){
    printf("Hello!\n");
    int s = (long) arg;
    int local = 0;
    for (int i = 0; i < NGOLD; i++){
        local += 1;
    }
    sem_wait(&s);
    //critical section
    sum += local;
    sem_post(&s);
    return NULL;
}


int main(){
    pthread_t threads[NTHREADS];
    sem_init(&s, 0, 1);
    for(int i = 0; i < NTHREADS; i++){
        pthread_create(&threads[i], NULL, countgold, (void *) i);
         
    }

    for(int i = 0; i < NTHREADS; i++){
        pthread_join(threads[i],NULL);
    }
    printf("sum: %d vs expected: %d \n", sum, NTHREADS*NGOLD);
}