// An attempt at a thread-safe stack (version 3)
/*There is no protection against underflow (popping on an empty stack) 
or overflow (pushing onto an already-full stack)*/
 /*is_empty is thread-safe but its result may already be out-of-date. 
 The stack may no longer be empty by the time the thread gets the result! 
 This is usually why in thread-safe data structures, 
 functions that return sizes are removed or deprecated.
*/
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#define STACK_SIZE 20
int count;
double values[STACK_SIZE];
pthread_mutex_t m1 = PTHREAD_MUTEX_INITIALIZER; 
void push(double v) { 
    pthread_mutex_lock(&m1); 
    values[count++] = v;
    pthread_mutex_unlock(&m1); }
double pop() { 
    pthread_mutex_lock(&m1);
    double v = values[--count]; 
    pthread_mutex_unlock(&m1);
    return v;
}
int is_empty() { 
    pthread_mutex_lock(&m1);
    int r = count == 0; 
    pthread_mutex_unlock(&m1);
    return r;
}