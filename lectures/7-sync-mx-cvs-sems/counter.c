/*written in lecture*/
#include <pthread.h>
#include <stdio.h>

int goldcount = 0;
int silvercount = 0;
pthread_mutex_t mgold = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t msilver = PTHREAD_MUTEX_INITIALIZER;
void *goldcounter(void *n) {
    int j;
    int local_count = 0;
   
    for (j = 0; j < ((long *)n); j++) {
   
       local_count++;
       
    }
    pthread_mutex_lock(&mgold);
    goldcount += local_count; 
    pthread_mutex_unlock(&mgold);
      
    return NULL;
}
void *silvercounter(void *n) {
    int j;
    int local_count = 0;
   
    for (j = 0; j < ((long *)n); j++) {
   
       local_count++;
       
    }
    pthread_mutex_lock(&msilver);
    pthread_mutex_lock(&mgold);
    silvercount += local_count; 
    goldcount += silvercount/10;
    pthread_mutex_unlock(&mgold);
    pthread_mutex_unlock(&msilver);
 
      
    return NULL;
}
int main(){
    #define NTHREADS  10
    //pthread_mutex_init(&m1)
    pthread_t goldtid[NTHREADS], silvertid[NTHREADS];
    for (int i = 0; i < NTHREADS; i++){
        pthread_create(&goldtid[i], NULL, goldcounter, (void *)1000000);
        pthread_create(&silvertid[i], NULL, silvercounter, (void *)1000000);
    }

    //
    for (int i = 0; i < NTHREADS; i++){
        pthread_join(silvertid[i], NULL);
        pthread_join(goldtid[i], NULL);
    }
    printf("butun threadler bu noktada bitti\n");
    printf("silvercount: %d, goldcount: %d\n", silvercount, goldcount);
    return 0;

}
