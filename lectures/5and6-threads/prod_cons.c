
#include <pthread.h>
#include <stdio.h>


#define N 8
pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cv = PTHREAD_COND_INITIALIZER;
unsigned count = 0;
void *consumer(void *ptr){
    pthread_mutex_lock(&m);
    while (count == 0)
        pthread_cond_wait(&cv, &m);
    count = count - 1;
    pthread_mutex_unlock(&m);
}

void *producer(void *ptr){
    pthread_mutex_lock(&m);
    count = count + 1;
    if (count > 0)
        pthread_cond_signal(&cv);
    pthread_mutex_unlock(&m);
}

int main(){
    pthread_t prod[N], cons[N];
    for (int i = 0; i < N; i++){
        pthread_create(&prod[i], NULL, producer,NULL);
        pthread_create(&cons[i], NULL, consumer,NULL);
    }
    
    printf("main thread\n");
    for (int i = 0; i < N; i++){
        pthread_join(cons[i], NULL);
        pthread_join(prod[i], NULL);
    }   
    printf("count: %d", count);
    return 0;
}