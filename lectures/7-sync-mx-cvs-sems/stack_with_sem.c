/* Simple single stack with semaphores
*/
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <semaphore.h>
#define SPACES 10

pthread_mutex_t m= PTHREAD_MUTEX_INITIALIZER; 
int count = 0;
double values[SPACES]; 
sem_t sitems, sremain;
void init() {
    sem_init(&sitems, 0, 0);
    sem_init(&sremain, 0, SPACES); // 10 spaces
}
double pop() {
    /* Wait until there’s at least one item*/
    sem_wait(&sitems);
    pthread_mutex_lock(&m); 
    /* CRITICAL SECTION */ 
    double v= values[--count];
    pthread_mutex_unlock(&m);
    sem_post(&sremain); /*Hey world, there’s at least one space*/
    return v;
}
void push(double v) {
    /* Wait until there’s at least one space*/
    sem_wait(&sremain);
    pthread_mutex_lock(&m); 
    /* CRITICAL SECTION */
    values[count++] = v; 
    pthread_mutex_unlock(&m);
    sem_post(&sitems); /* Hey world, there’s at least one item*/
}
