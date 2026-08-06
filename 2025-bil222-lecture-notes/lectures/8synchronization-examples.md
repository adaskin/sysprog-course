# Synchronization & Deadlock Examples  

*Re-formatted with DeepSeek-R1*

---

## Reader-Writer Problem

### Key Definitions
- **Readers**: Only read data (no modifications).
- **Writers**: Read and write data.

### Requirements
- Allow multiple concurrent readers when no writer is active.
- Writers require exclusive access (no readers/writers during write).
- **Goal**: Efficiently synchronize readers and writers.

---

## Initial Solutions & Issues

### First Attempt: Mutex
```c
void read() {
    lock(&m);
    // Read operation
    unlock(&m);
}

void write() {
    lock(&m);
    // Write operation
    unlock(&m);
}
```

---

- **Issue**: Only one reader/writer at a time. Readers block other readers.

---

### Second Attempt: Spinlocks
```c
void read() {
    while (writing) { /*spin*/ }
    reading = 1;
    // Read operation
    reading = 0;
}

void write() {
    while (reading || writing) { /*spin*/ }
    writing = 1;
    // Write operation
    writing = 0;
}
```

- **Issue**: Race conditions between readers and writers.

---

## Solution with Condition Variables
### Reader Code
```c
void read() {
    lock(&m);
    while (writing)
        cond_wait(&cv, &m);
    reading++;
    unlock(&m);

    // Read here (concurrent access allowed)

    lock(&m);
    reading--;
    cond_signal(&cv);
    unlock(&m);
}
```

---

### Writer Code
```c
void write() {
    lock(&m);
    while (reading || writing)
        cond_wait(&cv, &m);
    writing++;
    // Write here (exclusive access)
    writing--;
    cond_signal(&cv); // or cond_broadcast
    unlock(&m);
}
```
- **Mechanism**: 
  - `pthread_cond_wait` atomically releases the mutex and sleeps until signaled.
  - Awoken threads must re-acquire the mutex before proceeding.

---

## Problem: Starvation
- **Issue**: Continuous reader arrivals can block writers indefinitely.
- **Solution**: Prioritize writers using a "bounded wait" strategy.

---

## Final Solution (Writer Priority)
### Writer Code
```c
void writer() {
    lock(&m);
    writers++;
    while (reading || writing)
        cond_wait(&cv, &m);
    writing++;
    unlock(&m);

    // Write here

    lock(&m);
    writing--;
    writers--;
    cond_broadcast(&cv);
    unlock(&m);
}
```
---

### Reader Code
```c
void read() {
    lock(&m);
    while (writers) // Check waiting writers
        cond_wait(&cv, &m);
    while (writing) // Ensure no active writer
        cond_wait(&cv, &m);
    reading++;
    unlock(&m);

    // Read here

    lock(&m);
    reading--;
    cond_signal(&cv);
    unlock(&m);
}
```
---

- **Key Features**:
  - Writers register intent via `writers++`, forcing new readers to wait.
  - `cond_broadcast` ensures all waiting threads recheck conditions.
see also [pthread_rwlock](https://www.man7.org/linux/man-pages/man3/pthread_rwlock_rdlock.3p.html)

---

## Next Topics
- **Producer-Consumer Problem**: Ring buffer implementation with semaphores/condition variables.
- **Deadlock & Livelock**: Classic synchronization challenges.
- **Dining Philosophers Problem**: Solutions using resource hierarchy or timeout-based locks.

---

# Producer-Consumer Problem & Ring Buffer Implementation

---

## Review: Producer-Consumer Problem
- **Components**:
  - **Finite-size buffer**: Shared resource between producers and consumers.
  - **Producers**: Add items to the buffer (block if full).
  - **Consumers**: Remove items from the buffer (block if empty).
- **Synchronization Tools**:
  - **Condition variables**: Queue threads waiting for specific conditions (e.g., buffer space or items).

---

## Ring Buffer Basics
### Structure
- Fixed-size circular buffer with `in` (insertion index) and `out` (removal index).
- Example (non-thread-safe):

---

```c
void *buffer[16];
unsigned int in = 0, out = 0;

void enqueue(void *value) {
    buffer[in] = value;
    in = (in + 1) % 16; // Wrap around
}
```

```C
void *dequeue() {
    void *result = buffer[out];
    out = (out + 1) % 16;
    return result;
}
```
- **Issue**: No overflow/underflow checks.

---

## Incorrect Multithreaded Implementations

### Attempt 1: Broken Semaphore Usage
```c
sem_t s1, s2; // s1=16 (spaces), s2=0 (items)

void enqueue(void *value) {
    pthread_mutex_lock(&lock);
    sem_wait(&s1);
    buffer[(in++) & 15] = value;
    sem_post(&s1); // Wrong: s1 never decreases
    pthread_mutex_unlock(&lock);
}
```
---

```C
void *dequeue() {
    pthread_mutex_lock(&lock);
    sem_wait(&s2); // s2 starts at 0 → blocks forever
    void *result = buffer[(out++) & 15];
    sem_post(&s2);
    pthread_mutex_unlock(&lock);
    return result;
}
```
- **Issues**:
  - Semaphores `s1`/`s2` are reset within critical sections.
  - `s2` initialized to 0 causes immediate deadlock in `dequeue`.

---

### Attempt 2: Incorrect Semaphore Order
```c
void enqueue(void *value) {
    sem_wait(&s2); // Blocks immediately (s2=0)
    pthread_mutex_lock(&lock);
    buffer[(in++) & 15] = value;
    pthread_mutex_unlock(&lock);
    sem_post(&s1);
}
```

---

```C
void *dequeue() {
    sem_wait(&s1); // s1=16 → proceeds even if buffer empty
    pthread_mutex_lock(&lock);
    void *result = buffer[(out++) & 15];
    pthread_mutex_unlock(&lock);
    sem_post(&s2);
    return result;
}
```
- **Issues**:
  - `enqueue` blocks immediately due to `sem_wait(&s2)`.
  - `dequeue` proceeds with empty buffer (underflow).

---

## Correct Implementation with Semaphores

### Setup
```c
#define N 16 // Must be power of 2
void *b[N];
int in = 0, out = 0;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
sem_t countsem, spacesem; // Items & spaces

void init() {
    sem_init(&countsem, 0, 0); // Start with 0 items
    sem_init(&spacesem, 0, N); // Start with N spaces
}
```

---

### Enqueue (Producer)
```c
void enqueue(void *value) {
    sem_wait(&spacesem); // Wait for space
    pthread_mutex_lock(&lock);
    b[(in++) & (N-1)] = value;
    pthread_mutex_unlock(&lock);
    sem_post(&countsem); // Signal new item
}
```
---

### Dequeue (Consumer)
```c
void *dequeue() {
    sem_wait(&countsem); // Wait for item
    pthread_mutex_lock(&lock);
    void *result = b[(out++) & (N-1)];
    pthread_mutex_unlock(&lock);
    sem_post(&spacesem); // Signal free space
    return result;
}
```
- **Key**:
  - `spacesem` tracks available slots; `countsem` tracks items.
  - Mutex protects `in`/`out` indices.

---

## Correct Implementation with Condition Variables
### Buffer Structure
```c
typedef struct {
    char buf[BSIZE];
    int occupied; // Current items
    int in, out; // Indices
    pthread_mutex_t mutex;
    pthread_cond_t more; // Item available
    pthread_cond_t less; // Space available
} buffer_t;
```
---

### Enqueue (Producer)

```c
void enqueue(buffer_t *b, char item) {
    pthread_mutex_lock(&b->mutex);
    while (b->occupied >= BSIZE) 
        pthread_cond_wait(&b->less, &b->mutex); // Wait for space
    b->buf[b->in++] = item;
    b->in %= BSIZE;
    b->occupied++;
    pthread_cond_signal(&b->more); // Notify consumers
    pthread_mutex_unlock(&b->mutex);
}
```

---

### Dequeue (Consumer)
```c
char dequeue(buffer_t *b) {
    pthread_mutex_lock(&b->mutex);
    while (b->occupied <= 0)
        pthread_cond_wait(&b->more, &b->mutex); // Wait for items
    char item = b->buf[b->out++];
    b->out %= BSIZE;
    b->occupied--;
    pthread_cond_signal(&b->less); // Notify producers
    pthread_mutex_unlock(&b->mutex);
    return item;
}
```

**Reference**: [Oracle Thread Synchronization Guide](https://docs.oracle.com/cd/E19455-01/806-5257/6je9h032r/index.html#sync-ex-33)

---

## Exercises
1. **Order of Operations**:
   - What if `sem_wait` and `pthread_mutex_lock` are swapped in `enqueue`/`dequeue`?
   - What if `pthread_mutex_unlock` and `sem_post` are swapped?

2. **Analysis**:
   - Explain why mutexes are necessary even with semaphores.
   - How does the condition variable solution avoid starvation?


---

# Deadlock, Livelock, & Dining Philosophers Problem

---

## Deadlock
### Definition
A situation where two or more threads are **blocked forever**, each waiting for resources held by others.

### Example
- **Thread 1**: Holds Lock A, requests Lock B.
- **Thread 2**: Holds Lock B, requests Lock A.

---

### Conditions (Coffman Conditions)
1. **Mutual Exclusion**: Resources cannot be shared.
2. **Hold and Wait**: Processes hold resources while requesting new ones.
3. **No Preemption**: Resources cannot be forcibly taken.
4. **Circular Wait**: A cycle exists in the resource allocation graph.

---

## Livelock
- Threads **actively respond** to each other but make no progress.
- Example: Philosophers repeatedly picking up and putting down chopsticks.

---

## Starvation
- A thread is **unable to access shared resources** regularly, preventing progress.

---

## Resource Allocation Graphs
- **Cycle Detection**: Use algorithms like DFS to detect deadlocks.
- **Deadlock Occurs** if:
  - A cycle exists in the graph.
  - Each resource in the cycle has only one instance.

---

## Deadlock Solutions
### 1. Ostrich Algorithm
- Ignore deadlocks (common in many OS).
- "Pretend there’s no problem."

### 2. Banker’s Algorithm
- **Resource Tracking**:
  - **MAX**: Maximum resources a process may request.
  - **ALLOCATED**: Currently held resources.
  - **AVAILABLE**: System-wide free resources.

---

# Dining Philosophers Problem
### Scenario
- Five philosophers alternate between **thinking** and **eating**.
- **Chopsticks**: Shared between adjacent philosophers (one chopstick per philosopher initially).

### Problem
- **Deadlock**: All philosophers pick up their left chopstick simultaneously.
- **Livelock**: Philosophers repeatedly pick up/put down chopsticks.

---

## Example Code (Deadlock-Prone)
```c
void *philosopher(void *arg) {
    long i = (long)arg;
    pthread_mutex_t *left = &chopsticks[i];
    pthread_mutex_t *right = &chopsticks[(i + 1) % 5];

    while (1) {
        pthread_mutex_lock(left);   // Pick left
        pthread_mutex_lock(right);  // Pick right
        eat();
        pthread_mutex_unlock(right);
        pthread_mutex_unlock(left);
    }
}
```
**Deadlock Trigger**: All philosophers acquire left chopsticks first, then wait indefinitely for the right.

---

## Livelock Example
```c
void *philosopher2(void *arg) {
    // ...
    while (1) {
        if (pthread_mutex_trylock(left)) {
            sleep(1);
            continue;
        }
        if (!pthread_mutex_trylock(right)) {
            pthread_mutex_unlock(left); // Release left if right unavailable
            sleep(1);
            continue;
        }
        // Eat
        pthread_mutex_unlock(left);
        pthread_mutex_unlock(right);
    }
}
```
**Issue**: Philosophers may endlessly release/retry chopsticks (no progress).

---

## Solutions
### 1. Arbitrator (Waiter)
- Introduce a **central mutex** (waiter) to control chopstick access.
- **Drawback**: Reduced parallelism.

---

### 2. Hierarchical Ordering (Dijkstra's Solution)
- **Rule**: Philosophers pick **lower-numbered chopstick first**.
- **Example**:
  - Philosopher 4 (highest-numbered) picks chopstick 0 first (wraps around).
  - Breaks circular wait by enforcing order.

---

```c
void *philosopher_safe(void *arg) {
    long i = (long)arg;
    pthread_mutex_t *first = &chopsticks[MIN(i, (i+1)%5)];
    pthread_mutex_t *second = &chopsticks[MAX(i, (i+1)%5)];

    while (1) {
        pthread_mutex_lock(first);   // Lower-numbered first
        pthread_mutex_lock(second);
        eat();
        pthread_mutex_unlock(second);
        pthread_mutex_unlock(first);
    }
}
```
---

### 3. Limited Philosophers
- Restrict the number of philosophers allowed to eat simultaneously.

---

## Key Takeaways
- **Deadlock Prevention**:
  - Break one of the Coffman conditions (e.g., hierarchical ordering breaks circular wait).
- **Livelock Mitigation**: Introduce randomness or backoff mechanisms.
- **Starvation Avoidance**: Ensure fairness in resource allocation.

