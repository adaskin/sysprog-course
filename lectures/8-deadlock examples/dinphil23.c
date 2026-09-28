/*dining philosopher, written in lecture*/
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
/*
enum {
    hungry, eating, waiting,
} philstate;
struct phil{
    int id;
    enum pilstate state;
} phil;
*/
pthread_mutex_t chopsticks[5];
/**
 * a philospher:
 * with multi thread, deadlock occurs
 */
void *philosopher1(void *arg) {
  long i = (long)arg;
  pthread_mutex_t *left = &chopsticks[i];
  pthread_mutex_t *right = &chopsticks[(i + 1) % 5];

  while (1) {
    printf("id %ld: state: %.10s \n", i, "thinking!");
    // sleep(2);/*think()*/
    printf("id %ld: state: %.10s \n", i, "hungry!");
    pthread_mutex_lock(left);
    printf("id %ld: state: %.30s \n", i, "holding left, taking right!");
    sleep(1);

    pthread_mutex_lock(right);

    sleep(1); /*eat(left, right);*/

    printf("id %ld: state: %.10s \n", i, "eating");
    pthread_mutex_unlock(left);
    pthread_mutex_unlock(right);
    printf("id %ld: state: %.10s \n", i, "done!");
  }
}
/**
 * with trylock, livelock occurs
 */
void *philosopher2(void *arg) {
  long i = (long)arg;
  pthread_mutex_t *left = &chopsticks[i];
  pthread_mutex_t *right = &chopsticks[(i + 1) % 5];

  while (1) {
    printf("id %ld: state: %.10s \n", i, "thinking!");
    // sleep(2);/*think()*/
    printf("id %ld: state: %.10s \n", i, "hungry!");
    int l = pthread_mutex_trylock(left);
    if (!l) {
      sleep(1);
      continue;
    }

    printf("id %ld: state: %.30s \n", i, "holding left, taking right!");
    int r = pthread_mutex_trylock(right);
    if (!r) {
      pthread_mutex_unlock(left);
      sleep(1);
      continue;
    }
    // sleep(1); /*eat(left, right);*/

    printf("id %ld: state: %.10s \n", i, "eating");
    pthread_mutex_unlock(left);
    pthread_mutex_unlock(right);
    printf("id %ld: state: %.10s \n", i, "done!");
  }
}
/*TODO in Lab: phil with arbitrator*/
void *philosopher_with_arbitrator(void *arg) {
  long i = (long)arg;
  pthread_mutex_t *left = &chopsticks[i];
  pthread_mutex_t *right = &chopsticks[(i + 1) % 5];

  while (1) {
    printf("id %ld: state: %.10s \n", i, "thinking!");
    // sleep(2);/*think()*/
    printf("id %ld: state: %.10s \n", i, "hungry!");
    // arbitrator()
    // sleep(1); /*eat(left, right);*/

    printf("id %ld: state: %.10s \n", i, "eating");
    // arbitrator()
    printf("id %ld: state: %.10s \n", i, "done!");
  }
}

void *dijkstrasalgorithm(void *arg) {
  long i = (long)arg;
  int a = i, b = (i + 1) % 5;
  int f = (a < b) ? a : b; 
  int s = (a < b) ? b : a;
  pthread_mutex_t *first = &chopsticks[f];
  pthread_mutex_t *second = &chopsticks[s];

  while (1) {
    printf("id %ld: state: %.10s \n", i, "thinking!");
    // sleep(2);/*think()*/
    printf("id %ld: state: %.10s \n", i, "hungry!");
    pthread_mutex_lock(first);
    // sleep(1);
    pthread_mutex_lock(second);

    // sleep(1); /*eat(left, right);*/

    printf("id %ld: state: %.10s \n", i, "eating");
    pthread_mutex_unlock(first);
    pthread_mutex_unlock(second);
    printf("id %ld: state: %.10s \n", i, "done!");
  }
}

int main() {
  pthread_t threads[5];

  /*initialize mutex locks*/
  for (int i = 0; i < 5; i++) {
    pthread_mutex_init(&chopsticks[i], NULL);
  }

  for (long i = 0; i < 5; i++) {
    pthread_create(&threads[i], NULL, philosopher2, (void *)i);
  }

  for (int i = 0; i < 5; i++) {
    pthread_join(threads[i], NULL);
  }
}