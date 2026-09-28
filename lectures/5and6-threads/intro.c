#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
struct data{
    char arg1;
    int arg2;
} data1={'1', 2};/*SHARED MEMORY: SYNCRONIZATION NECESSARY*/
void *routine(void *arg){
    struct data* d = (struct data *) arg;
    
    printf("Hello from thread: %c-%d\n", d->arg1, d->arg2);
    //sleep(5);
    pthread_exit(0);
}
int main(){
    pthread_t tid;

    int r = pthread_create(&tid, NULL, routine, (void *)&data1);
    if(r == -1) perror("thread create:");
    pthread_join(tid, NULL);

    printf("after the thread\n");
    return 0;
}
