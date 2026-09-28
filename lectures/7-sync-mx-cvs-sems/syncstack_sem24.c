/*written in lecture for educaitonal purposes
*/
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>

typedef struct stack {
   int count;
   pthread_mutex_t lock;/**/
   sem_t sem1;
   sem_t sem2;
   double *values;
   int capacity;
} stack_t;
stack_t *create_stack(int capacity){
    stack_t *s = malloc(sizeof(stack_t));
    s->values = malloc(sizeof(double)*capacity);
    s->count = 0;
    pthread_mutex_init(&s->lock,NULL);
    s->capacity = capacity;
    sem_init(&s->sem1, 0, capacity);
    sem_init(&s->sem1, 0, 0);
    
    return s;
}
destroy(stack_t *s){
    free(s->values);
    free(s);
}
double pop(stack_t *s){
    sem_wait(s->sem2);
    pthread_mutex_lock(&s->lock);
    double item = s->values[s->count-1];
    
    //free(s->values[(s->count-1)]);
    s->count--;
   
    pthread_mutex_unlock(s->lock);
    sem_post(s->sem1);
    
    return item;

}
void push(stack_t *s, double d){
    sem_wait(&s->sem1);
    pthread_mutex_lock(&s->lock);

    s->values[++s->count] = d;
    pthread_mutex_lock(&s->lock);
    sem_post(&s->sem2);
    return;
}


double peek(stack_t *s){
    pthread_mutex_lock(&s->lock);
    double item = s->values[s->count];
    pthread_mutex_lock(&s->lock);
    return item;

}
is_empty(){
    pthread_mutex_lock(&s->lock);
    int c = s->count;
    pthread_mutex_lock(&s->lock);
    return c;
}