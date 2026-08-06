// An attempt at a thread-safe stack (version 3)
/*with dynamic memory taken from the book
*/
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#define STACK_SIZE 20
int count;

typedef struct stack {
    int count;
    pthread_mutex_t m;
    double *values; 
} stack_t; 
stack_t* stack_create(int capacity) { 
    stack_t *result = malloc(sizeof(stack_t));
    result->count = 0;
    result->values = malloc(sizeof(double) * capacity); 
    pthread_mutex_init(&result->m, NULL);
    return result;
}
void stack_destroy(stack_t *s) {
    free(s->values); 
    pthread_mutex_destroy(&s->m);
    free(s);
}
/* !!!!Warning no underflow or overflow checks!
this can be fixed by using counting semaphores*/
void push(stack_t *s, double v) {
    pthread_mutex_lock(&s->m); 
    s->values[(s->count)++] = v; 
    pthread_mutex_unlock(&s->m);
}
double pop(stack_t *s) {
    pthread_mutex_lock(&s->m);
    double v = s->values[--(s->count)]; 
    pthread_mutex_unlock(&s->m);
    return v; 
}
int is_empty(stack_t *s) {
    pthread_mutex_lock(&s->m); 
    int result = s->count == 0; 
    pthread_mutex_unlock(&s->m);
    return result;
}
int main() {
    stack_t *s1 = stack_create(10 /* Max capacity*/);
    stack_t *s2 = stack_create(10); 
    push(s1, 3.141);
    push(s2, pop(s1));
    stack_destroy(s2);
    stack_destroy(s1); 
}