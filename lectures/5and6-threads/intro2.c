#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
struct data {
    int i;
    int *pi;
};

#define NTHREADS 10
char buf[100]; /*shared memory*/
pthread_t tid[NTHREADS];
/*multi thread*/
void *printHi(void *arg){
    char locbuf[1000]; /*private memory*/

    int start = 0, end = 10;
    //exit(0);
    //pthread_join(..., NULL); /*burdada olabilir. hiyerarsi yok*/

    for(int i = start; i < end; i++ ){
        printf("Hello from thread-id: %d arg%ld\n", pthread_self(), (long) arg);
        sleep(1);
    }
    
    pthread_exit(0);

}
void *userInput(void *arg){
    char s[100] = {'\0'};
    scanf("%100s",s);
}
int main(){


    for (int i = 0; i < NTHREADS; i++){
        int r = pthread_create(&tid[i], NULL, printHi, (void *)i); /*shared memory olmamali*/
        if(r == -1) perror("thread create:");

        /*pthread_join(tid[i], NULL);*//*BLOCKladigi icin HATA!!, serial computing oluyor*/
    }
    for(int i = 0; i < NTHREADS; i++){
        pthread_join(tid[i], NULL);
    }
    printf("after the thread\n");
    return 0;
}
