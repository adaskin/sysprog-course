#include <pthread.h>
#include <stdio.h>
char *messages[8];
int sum = 0;

#define N 8
#define NUM  10000000
pthread_mutex_t m1 = PTHREAD_MUTEX_INITIALIZER, 
                m2 = PTHREAD_MUTEX_INITIALIZER;
void *printHello(void *arg){
    
    int local_sum = 0;
    for (int i = 0; i<(long) arg; i++){
        local_sum += 1;
    }
    pthread_mutex_lock(&m1);
    sum +=  local_sum;
    pthread_mutex_unlock(&m1);

    pthread_exit(NULL);//race condition
}

void *printHello2(void *arg){
    for (int i = 0; i<(long) arg; i++){
        sum += 1;
    }   
    pthread_exit(NULL);
}


int main()
{
    pthread_t tid[N];
    for (int i = 0; i < N; i++){
        pthread_create(&tid[i], NULL, printHello2,(void *)NUM);
        //pthread_join(tid[i], NULL);//wait-block
    }
    
    printf("main thread\n");
    for (int i = 0; i < N; i++){
        pthread_join(tid[i], NULL);//wait-block
    }   
    printf("sum: %d", sum);
    return 0;
}

