# Synchronization

---

## Key Concepts

- **Synchronization Mechanisms**:
  - Mutex
  - Condition Variables
  - Monitors
  - Semaphores
  - Thread-Safe Data Structures
- The content is primarily derived from [Chapter 7 of the Course Book](https://github.com/angrave/SystemProgramming/wiki#5-synchronization).
- Algorithmic and kernel details will also be explored in **BIL 301 Operating Systems**.

---

### When Multithreading Gets **“Interesting”** 🍿

```c
#include <stdio.h>
#include <pthread.h>

int sum = 0; // shared variable

void *countgold(void *param) {
    int i; // local to each thread
    for (i = 0; i < 10000000; i++) {
        sum += 1;
    }
    return NULL;
}

int main() {
    pthread_t tid1, tid2;
    pthread_create(&tid1, NULL, countgold, NULL);
    pthread_create(&tid2, NULL, countgold, NULL);

    // Wait for both threads to finish:
    pthread_join(tid1, NULL);
    pthread_join(tid2, NULL);

    printf("ARRRRG sum is %d\n", sum);
    return 0;
}
```

---

### Run & Compile 🚀

```bash
$ gcc main.c -pthread
$ ./a.out 
??????????
```

---

Every run may result in different outputs:

```bash
$ ./a.out 
ARRRRG sum is 10214142
$ ./a.out 
ARRRRG sum is 11798888
$ ./a.out 
ARRRRG sum is 10576703
$ ./a.out 
ARRRRG sum is 10159994
```

---

### What’s the Issue? 🤔

There is clearly a **race condition** when the end result changes based on how instructions are interleaved between threads.

---

## Thread Synchronization

### Mutex (**Mutual Exclusion**) 🛡️

- Use a **mutex** to protect access to shared variables.
- Ensures that only **one thread** can modify a shared variable at a time.

---

### Condition Variables 🛎️

- Inform another thread when the state of a shared variable changes.
- Synchronize based on the **actual value** of the variable.

---

### Critical Section

From the previous example:  
We do not want **two or more threads** to update `sum` at the same time.

**Definition**:  
Critical section is the part of the code where shared resources (variables) are accessed.

---

## **Mutual Exclusion**

### Key Property:

> **“No two or more threads should be in their critical section at the same time.”**

More on this to come in the **Operating Systems** course.

---

## Implementing Mutual Exclusion with Mutexes

In POSIX, use `pthread_mutex_t`.

### Example: Mutex Basics

```c
/* Global variable should be used for static mutexes */
pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;

/* Start of Critical Section */
pthread_mutex_lock(&m);
// Critical section code here...
pthread_mutex_unlock(&m);
/* End of Critical Section */
```

---

### Conceptual Analogy

Think of a mutex as a “mic” among participants.  
- **Whoever holds the mic gets to speak.**
- Similarly, **whoever holds the mutex gets access to the shared memory.**

---

### Dynamic Mutex Allocation

```c
pthread_mutex_t *lock = malloc(sizeof(pthread_mutex_t));
pthread_mutex_init(lock, NULL);
// Critical Section
pthread_mutex_lock(lock);
// Perform operations
pthread_mutex_unlock(lock);
pthread_mutex_destroy(lock);
free(lock);
```

---

### Optimized Example: Summing Numbers

#### Without Mutex (Race Condition)

```c
void *countgold(void *param) {
    for (int i = 0; i < 10000000; i++) {
        sum += 1; // Critical Section
    }
    return NULL;
}
```

---

#### Basic Mutex Use

```c
void *countgold(void *param) {
    pthread_mutex_lock(&m);
    for (int i = 0; i < 10000000; i++) {
        sum += 1; // Critical Section
    }
    pthread_mutex_unlock(&m);
    return NULL;
}
```

---

#### Improved Efficiency with Local Variables

```c
void *countgold(void *param) {
    int local = 0;
    for (int i = 0; i < 10000000; i++) {
        local += 1; // Local work
    }
    pthread_mutex_lock(&m);
    sum += local; // Update global sum
    pthread_mutex_unlock(&m);
    return NULL;
}
```

> **Which implementation runs faster?** 🚀  
> Definitely the last one, as it reduces the overhead of frequently locking/unlocking the mutex.

---

## Example: Sum of Elements in an Array

Write a concurrent program where:
1. Declare a **global sum**.
2. Divide array sections between threads (`start` and `end` based on thread index).
3. Each thread computes its partial sum.
4. Combine all partial sums into the global sum.

---

### **Some Common Gotchas 🤯**

#### 1. Ineffective or Redundant Mutexes

```c
int a;
pthread_mutex_t m1 = PTHREAD_MUTEX_INITIALIZER,
                m2 = PTHREAD_MUTEX_INITIALIZER;

// Thread 1
pthread_mutex_lock(&m1);
a++;
pthread_mutex_unlock(&m1);

// Thread 2
pthread_mutex_lock(&m2);
a++;
pthread_mutex_unlock(&m2);
```

---

**Problem**: This code creates two mutexes (`m1`, `m2`), but neither ensures mutual exclusion!

---

#### 2. Small Overhead

There’s always a small amount of overhead when calling mutex functions. This is the **price of correctness**.

---

#### 3. Potential Errors in Mutex Use

- Failing to unlock a mutex (e.g., during an early return/error handling).
- Not destroying a mutex (resource leak).
- Using an uninitialized mutex or a destroyed mutex.
- Locking a mutex twice without unlocking it (can lead to **deadlock**).

---

### Designing with Mutexes

1. Designate **one lock per shared resource** rather than reusing a single lock for all resources.
2. Avoid contention by using multiple locks (e.g., **per data structure**).

---

## Important Reminder

Mutexes are **tools**, not a guarantee to identify or solve race conditions. Always carefully design critical sections! 📐

> **“Don’t cross the streams!”**  
> If using threads, avoid forking in the middle of your program after mutexes have been initialized.

---


# Condition Variables: Overview 💡

---

## Mutexes 🛡️

- **Purpose**:
  - Synchronization by controlling thread access to shared data.
  - Prevent multiple threads from modifying the shared variable **at the same time**.

- **Thread Participation**:
  - Threads must explicitly **acquire and release the mutex** to participate in the synchronization process.

---

## Condition Variables 🛎️

- **Purpose**:
  - Allow synchronization based on the **actual value** of shared data.
  - Enable one thread to:
    - Inform other threads about changes in the state of shared data/resources.
    - Allow threads to **wait (block)** until notified.
  - Facilitate a group of threads to **sleep until woken up**.

- **Key Rule**:
  Condition variables are always used in **conjunction with a mutex lock**.

---

## Monitors 📡

### **Definition**:

A **monitor** is:
- A **synchronization construct** (or "pattern").
- Contains:
  1. A **mutex (lock)** to protect the critical section.
  2. At least **one condition variable**, allowing threads to wait for specific state changes.

### **Purpose**:
- Temporarily **give up exclusive access** to resources when waiting for certain conditions to be met.

---

### Basic Monitor Construct

```c
lock(m);            // Acquire this monitor's lock.
/* Critical Section */
while (!p) {        // While the condition is not true
    wait(cv, m);    // Wait using the mutex and condition variable.
}
/* Critical Section */
signal(cv2);        // Signal or broadcast to wake waiting threads.
unlock(m);          // Release this monitor's lock.
```

---

### Two Threads Example 🌐

#### **Thread 1**

```c
mutex_lock(m1);
/* Critical section entry */
while (need_to_wait_1) {
    cond_wait(c1, m1);
}
/* Critical section exit */
cond_signal(c2);  // Or: cond_broadcast(c2);
mutex_unlock(m1);
```

---

#### **Thread 2**

```c
mutex_lock(m1);
/* Critical section entry */
while (need_to_wait_2) {
    cond_wait(c2, m1);
}
/* Critical section exit */
cond_signal(c1);  // Or: cond_broadcast(c1);
mutex_unlock(m1);
```

- `m1`: Mutex lock to control access to the critical section.
- `c1` & `c2`: Condition variables, one for each condition.

---

## Creating Condition Variables

### **Declaration and Initialization**

1. **Static Initialization**:
   ```c
   pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
   ```

2. **Dynamic Initialization**:
   ```c
   int pthread_cond_init(pthread_cond_t *cond, pthread_condattr_t *attr);
   ```
   - `attr`: Allows setting custom attributes for the condition variable.
   - Always destroy dynamically initialized condition variables:
     ```c
     int pthread_cond_destroy(pthread_cond_t *cond);
     ```

---

## Signaling and Waiting on Condition Variables

### **APIs**

```c
#include <pthread.h>

int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex);
// Puts the thread to sleep until the condition is signaled.
// Note: Unlocks the mutex while waiting and re-locks it before returning.

int pthread_cond_signal(pthread_cond_t *cond);
// Wakes ONE waiting thread.

int pthread_cond_broadcast(pthread_cond_t *cond);
// Wakes ALL waiting threads.
```

---

### **Key Points**:
- Always **acquire the mutex lock before waiting**:
  ```c
  pthread_mutex_lock(&mutex);
  while (condition_is_false) {
      pthread_cond_wait(&cond, &mutex);
  }
  pthread_mutex_unlock(&mutex);
  ```

- Use a **while loop** to protect against **spurious wakeups** or race conditions.

---

## Producer-Consumer Problem 🍎➡️🍽️

### Example Without Condition Variables (Inefficient)

```c
pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
int avail = 0;

void *producer(void *p) {
    pthread_mutex_lock(&mtx);
    avail++; 
    pthread_mutex_unlock(&mtx);
}

int main() {
    pthread_mutex_lock(&mtx);
    while (avail > 0) { 
        avail--;
        // Busy looping (inefficient)
    }
    pthread_mutex_unlock(&mtx);
}
```

---

#### **Problem**:
- The above code wastes CPU cycles with **busy-waiting**, continuously polling `avail`.

---

### Example With Condition Variables (Efficient)

```c
pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cv = PTHREAD_COND_INITIALIZER;
int avail = 0;

// Producer
void *producer(void *p) {
    pthread_mutex_lock(&mtx);
    avail++;
    pthread_cond_signal(&cv);  // Notify consumer
    pthread_mutex_unlock(&mtx);
}

// Consumer
void *consumer(void *p) {
    for (;;) {
       pthread_mutex_lock(&mtx);

       // Wait while `avail` is 0
       while (avail == 0) {
           pthread_cond_wait(&cv, &mtx);
       }

       // Consume the available resource
       avail--;

       pthread_mutex_unlock(&mtx);
    }
}
```

---

### Explanation:
- `pthread_cond_wait()` automatically:
  - Unlocks the mutex while waiting.
  - Re-locks the mutex when the thread is woken.

---

### Producer-Consumer with Overflow Control 🛢️

```c
#define BUFFER_SIZE 10
pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t notempty = PTHREAD_COND_INITIALIZER;
pthread_cond_t notfull = PTHREAD_COND_INITIALIZER;

int count = 0;
int buffer[BUFFER_SIZE];

// Producer
void *producer(void *ptr) {
    pthread_mutex_lock(&m);
    while (count >= BUFFER_SIZE) {  // Buffer full
        pthread_cond_wait(&notfull, &m);  // Wait for space
    }

    // Produce item
    buffer[count] = rand();
    printf("Producing item %d\n", count);
    count++;

    pthread_cond_signal(&notempty);  // Notify consumer
    pthread_mutex_unlock(&m);
}

// Consumer
void *consumer(void *ptr) {
    pthread_mutex_lock(&m);
    while (count == 0) {  // Buffer empty
        pthread_cond_wait(&notempty, &m);  // Wait for items
    }

    // Consume item
    count--;
    printf("Consumed item %d - %d\n", count, buffer[count]);

    pthread_cond_signal(&notfull);  // Notify producer
    pthread_mutex_unlock(&m);
}
```

---

#### **Key Features**:
- **Buffer Overflow Protection**:
  - Producer waits if the buffer is full.
- **Buffer Underflow Protection**:
  - Consumer waits if the buffer is empty.

---

### Design Considerations

1. **Multiple Resources**:
   - Use **separate mutexes** for different shared resources to reduce contention.

2. **Processes**:
   - For inter-process synchronization, consider using semaphores or shared memory.

---

### Summary 📃

- **Condition Variables**: Enable efficient thread synchronization by waiting for specific conditions to be met.
- **Mutex**: Ensures mutual exclusion around shared resources.
- Use **monitor constructs** to combine condition variables and mutexes for robust synchronization.


---

# 🔒 Semaphores: Overview

```c
sem_t s;
sem_init(&s, 0, 10); // 10 available resources (e.g., 10 chairs)

// Thread acquiring a resource
sem_wait(&s);  // Decrements s by 1
// ... use resource ...
sem_post(&s);  // Increments s by 1
```

- **Blocking Behavior**: If `sem_wait` reduces the value to **<0**, the thread waits (like mutex locks).
- **Cross-Thread/Process**: `sem_wait`/`sem_post` can be called from different threads/processes.

---

# 🛠️ POSIX Semaphores

### Initialization
```c
int sem_init(sem_t *sem, int pshared, unsigned int value);
```
- `pshared`:  
  - `0`: Shared between **threads** (global variable).  
  - `Non-zero`: Shared between **processes**.  
- `value`: Initial semaphore count.

### Operations
- **Decrement** (Wait): `sem_wait(&s)`  
  - "P" operation (Dijkstra’s terminology).  
- **Increment** (Post): `sem_post(&s)`  
  - "V" operation.  
- **Destroy**: `sem_destroy(&s)` releases resources.

---

# 🔄 Binary Semaphore vs. Mutex

| **Binary Semaphore**           | **Mutex**                          |
|---------------------------------|------------------------------------|
| Max count = 1                   | Always waits before posting.       |
| No thread ownership tracking.  | Locking thread **must** unlock it. |
| `sem_post` from any thread.    | `mutex_unlock` only by owner.      |

⚠️ **Example Pitfall**  
```c
// Thread 2 (rogue):
sem_post(&s);  // Unlocks semaphore even if not the owner!
```

---

# 🧵 Making Data Structures Thread-Safe

### Thread-Unsafe Stack (Example)
```c
int count = 0;
double values[STACK_SIZE];

void push(double v) { values[count++] = v; }
double pop() { return values[--count]; }
```
**Issue**: Concurrent `push`/`pop` calls corrupt `count` and values.

---

# 🚫 Version 2: Flawed Thread-Safe Stack

```c
pthread_mutex_t m1, m2;  // Two mutexes!

void push(double v) {
  pthread_mutex_lock(&m1);
  values[count++] = v;
  pthread_mutex_unlock(&m1);
}

double pop() {
  pthread_mutex_lock(&m2);  // Error: Separate mutex for pop!
  double v = values[--count];
  pthread_mutex_unlock(&m2);
  return v;
}

int is_empty() {
  pthread_mutex_lock(&m1);
  return count == 0;  // Error: Unlock skipped after return!
}
```

---

### 🔍 **Errors**  
1. **Separate mutexes** (`m1`/`m2`) → `count` is not protected.  
2. `is_empty` **never unlocks** the mutex after `return`.  

---

# ✅ Version 3: Improved Thread-Safe Stack

```c
pthread_mutex_t m1 = PTHREAD_MUTEX_INITIALIZER;

void push(double v) {
  pthread_mutex_lock(&m1);
  values[count++] = v;  // No overflow check!
  pthread_mutex_unlock(&m1);
}

int is_empty() {
  pthread_mutex_lock(&m1);
  int r = (count == 0);
  pthread_mutex_unlock(&m1);
  return r;  // Result may be outdated!
}
```
**Limitations**:  
- No overflow/underflow protection.  
- `is_empty()` result is **not atomic** after unlock.  

---

# 🏗️ General-Purpose Stack with Mutex

```c
typedef struct stack {
  int count;
  pthread_mutex_t m;
  double *values;
} stack_t;

stack_t* stack_create(int capacity) {
  stack_t *s = malloc(sizeof(stack_t));
  s->count = 0;
  s->values = malloc(sizeof(double) * capacity);
  pthread_mutex_init(&s->m, NULL);
  return s;
}

void push(stack_t *s, double v) {
  pthread_mutex_lock(&s->m);
  s->values[s->count++] = v;  // Still no overflow check!
  pthread_mutex_unlock(&s->m);
}
```

---

# 🚦 Semaphore-Protected Stack

```c
sem_t sitems, sremain;  // Items and spaces

void init() {
  sem_init(&sitems, 0, 0);
  sem_init(&sremain, 0, SPACES);
}

void push(double v) {
  sem_wait(&sremain);    // Wait for space
  pthread_mutex_lock(&m);
  values[count++] = v;
  pthread_mutex_unlock(&m);
  sem_post(&sitems);     // Notify new item
}

double pop() {
  sem_wait(&sitems);     // Wait for item
  pthread_mutex_lock(&m);
  double v = values[--count];
  pthread_mutex_unlock(&m);
  sem_post(&sremain);    // Notify free space
  return v;
}
```

---

# ⚠️ Condition Variable Pitfalls

### Wrong Implementation (Spurious Wakeups)
```c
// Push (flawed):
pthread_mutex_lock(&m);
if (count == capacity) pthread_cond_wait(&cv, &m);  // Should be 'while'!
values[count++] = v;
pthread_mutex_unlock(&m);
```

### Correct Approach
```c
// Push:
pthread_mutex_lock(&m);
while (count == capacity) pthread_cond_wait(&cv1, &m);  // Use 'while'!
values[count++] = v;
pthread_cond_signal(&cv2);  // Alert consumers
pthread_mutex_unlock(&m);
```

---

# 🚧 Barriers

```c
pthread_barrier_t barrier;

// Initialize (e.g., 4 threads):
pthread_barrier_init(&barrier, NULL, 4);

// Threads synchronize here:
pthread_barrier_wait(&barrier);  // All threads block until 4 arrive
```

**Use Case**: Synchronize multi-stage parallel computations.

---

# Next Topics
- **Reader/Writer Problem**  
- **Deadlock Examples**  
```c
// Classic deadlock:
Thread 1: lock(A); lock(B);
Thread 2: lock(B); lock(A);
```