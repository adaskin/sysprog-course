#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct stack {
   int count;
   pthread_mutex_t lock;/**/
   pthread_cond_t notempty;
   pthread_cond_t notfull;
   double *values;
   int capacity;
} stack_t;
stack_t *create_stack(int capacity){
    stack_t *s = malloc(sizeof(stack_t));
    s->values = malloc(sizeof(double)*capacity);
    s->count = 0;
    pthread_mutex_init(&s->lock, NULL);
    s->capacity = capacity;
    return s;
}
desstroy(stack_t *s){
    free(s->values);
    free(s);
}
double pop(stack_t *s){
    pthread_mutex_lock(&s->lock);
    while(s->count == 0){
        pthread_cond_wait(&s->notempty,&s->lock);
    }
    double item = s->values[s->count-1];
    
    free(s->values[(s->count-1)]);
    s->count--;
    pthread_cond_signal(&s->notfull);
    pthread_mutex_unlock(&s->lock);
    return item;

}
void push(stack_t *s, double d){
    pthread_mutex_lock(&s->lock);
    while(s->count >= s->capacity){
        pthread_cond_wait(&s->notfull, &s->lock);
    }
    s->values[++s->count] = d;
    pthread_cond_signal(&s->notempty);
    pthread_mutex_lock(&s->lock);
    return;
}


double peek(stack_t *s){
    pthread_mutex_lock(s->lock);
    double item = s->values[s->count];
    pthread_mutex_lock(s->lock);
    return item;

}
is_empty(){
    pthread_mutex_lock(s->lock);
    int c = s->count;
    pthread_mutex_lock(s->lock);
    return c;
}