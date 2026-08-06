// An attempt at a thread-safe stack (version 2)
/*Version 2 does not prevent push and pop from running
 at the same time because push and pop use two different mutex locks.*/
 /*The code has a second error. 
 is_empty returns after the comparison and leaves the mutex unlocked.*/
 #include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#define STACK_SIZE 20
int count;
double values[STACK_SIZE];
pthread_mutex_t m1 = PTHREAD_MUTEX_INITIALIZER; 
pthread_mutex_t m2 = PTHREAD_MUTEX_INITIALIZER;
void push(double v) { 
    pthread_mutex_lock(&m1); 
    values[count++] = v;
    pthread_mutex_unlock(&m1); }
double pop() { 
    pthread_mutex_lock(&m2);
    double v = values[--count]; 
    pthread_mutex_unlock(&m2);
    return v;
}
int is_empty() { 
    pthread_mutex_lock(&m1);
    return count == 0; 
    pthread_mutex_unlock(&m1);
}