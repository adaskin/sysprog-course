#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#define N 10000
int a[N];
struct tinfo{
    int *start; int *end; int x;
};
int sum = 0;
pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
void *add(void *par){
    struct tinfo *sp = (struct tinfo *)par;
    for(int *p = sp->start; p <= sp->end; p++){
        pthread_mutex_lock(&m);
        sum += *p;
        pthread_mutex_unlock(&m);
    }
    pthread_exit(NULL);
}
int main(){
    
    for(int i = 0; i<N; i++){
        a[i] = 1;
    }

    struct tinfo sp = {&a[0], &a[N/2], a[1]};
    struct tinfo sp2 = {&a[N/2+1], &a[N-1], a[1]};
    pthread_t t, t2;
    pthread_create(&t,NULL, add, &sp);
    pthread_create(&t2,NULL, add, &sp2);
    pthread_join(t, NULL);
    pthread_join(t2, NULL);
    printf("sm = %d", sum);
    return 0;
}