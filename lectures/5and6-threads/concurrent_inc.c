#include <stdio.h>
#include <pthread.h>

int count = 0;
pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
void *increment(void *pn) {
  int j;
  for (j = 0; j < *((int *)pn); j++) {
    pthread_mutex_lock(&m);
    count++;
    pthread_mutex_unlock(&m);
   }
}
int main(){
    int n = 100000;
    pthread_t t1, t2;
    pthread_create(&t1, NULL, increment, &n);
    pthread_create(&t2, NULL, increment, &n);
    pthread_join(t1,NULL);
    pthread_join(t2,NULL);
    printf("count= %d", count);    
    
}