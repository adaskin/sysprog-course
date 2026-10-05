---
title: "memory management"
author: "Ammar Daskin"
marp: true
paginate: true
size: 16:10
theme: default
class: invert
---
# Basic Memory Management  
**System-Level Memory Operations**
 Memory allocation/deallocation is done by almost any application

---

##  Heap Fundamentals  
### Program Memory Lifecycle

```plaintext
[Code]━━[Data]━━[Heap]━━━▶[ ← Program Break → ] ◀━━[Stack]
```

>The **heap** at the system level is a contiguous series of addresses that the program can expand or contract

**Heap Dynamics:** Contiguous address space expandable via system calls
**Unix Specific:**
- 🔧 `brk()` - Directly sets program break address
- 🔄 `sbrk()` - Adjusts program break by incremental value

---

## C Memory API Essentials
## Core Allocation Functions
```c
#include <stdlib.h>

/* Allocate SIZE bytes of uninitialized memory */
void *malloc(size_t size);
```

---

**Safe Allocation Pattern:**
```c
int *ptr = (int*)malloc(sizeof(int));
if (!ptr) {
    fprintf(stderr, "Allocation failed!\n"); 
    exit(EXIT_FAILURE);
}
```


**Critical Note**:  
❗ Always verify return value before usage  
❗ Use `memset` for initialization when needed  


---

### Struct Allocation Example

```c
struct foo *ptr;
ptr = (struct foo *) malloc(sizeof(struct foo));
if (ptr == 0) abort();
memset(ptr, 0, sizeof(struct foo));  // Zero initialization

// Array of pointers
int *ptrs[10];  
ptrs[0] = malloc(sizeof(int));
ptrs[1] = malloc(sizeof(int));


// Double pointer allocation
int **dbl_ptr = malloc(sizeof(int*));
```

---

### A simple Implementation of `calloc()`


```c

void *calloc(size_t nmemb, size_t size) {
    size_t total = nmemb * size;
    void *mem = malloc(total);
    return mem ? memset(mem, 0, total) : NULL;
}

```
**Advantages**:  

✅ Automatic zero-initialization  
✅ Built-in overflow check (modern implementations)  

---

### Changing the size of the allocated memory

```c
void *xrealloc(void *ptr, size_t size) {
  void *value = realloc(ptr, size);

  if (value == 0)
      perror("Virtual memory exhausted");
  return value;
}
```


---

### `free()`
**Functionality:**
1. Usually, allows a later call to malloc to reuse the space.
 2. In the meantime, the space remains in your program as part of a free-list used internally by malloc.
 3. Occasionally, free can actually return memory to the operating system and make the process smaller.
 4. Any use of a pointer that refers to freed space results in undefined behavior.

---

```c
int *ptr = malloc(sizeof(*ptr));
do_something(ptr);
free(ptr);
```

---

### Struct Freeing Strategy

```c
struct chain {
    struct chain *next;
    char *name;
};

void free_chain(struct chain *head) {
    while (head) {
        struct chain *temp = head;
        head = head->next;
        free(temp->name);  // Free contained data first
        free(temp);
    }
}
```
**Golden Rule**:  
🔁 Free child resources before parent structure  

---

### realloc()
Changes the memory block size to newsize bytes.
```c
/* simple implementation without error handling */
void *realloc(void *ptr, size_t newsize) {
    void *result = malloc(newsize);
    size_t oldsize = ...;  /* Depends on allocator*/
    if (ptr) {
        memcpy(result, ptr, newsize < oldsize ? newsize : oldsize);
    }
    free(ptr);
    return result;
}
```
> Potential Problem: Memory Leak on Failure

---

```c
int main() {
    int *array = malloc(sizeof(int) * 2);
    array[0] = 10;
    array[1] = 20;

    /* Resize with realloc */
    array = realloc(array, 3 * sizeof(int));
    array[2] = 30;
}
```
> Memory Leaks

---


**Improved Version**:

```c
int main() {
    int *array = malloc(sizeof(int) * 2);
    array[0] = 10;
    array[1] = 20;

    void *tmp = realloc(array, 3 * sizeof(int));

    if (tmp == NULL) {
        /* Handle memory allocation failure */
    } else if (tmp == array) {
        /* Realloc returned the same space */
        array[2] = 30;
    } else {
        /* Realloc returned different space */
        array = tmp;
        array[2] = 30;
    }
}
```

---

## Heap and sbrk() System Call

**Key Notes:**
    • The heap is part of process memory and varies in size.
    • Memory allocation is done via `malloc`, `calloc`, and `realloc`.
    • sbrk() increases the heap size as required.
**Modern Usage:**
    • Modern systems prefer virtual memory management using `mmap` over `sbrk`.

```plaintext
Call to `sbrk` is still occasionally used for small memory requests.
```

---

Changing the Program Break
```c
#include <unistd.h>

// Set the end of process's data segment
int brk(void *end_data_addr);

// Increase the end of data segment
void *sbrk(intptr_t increment);

```

```c
// Example:
void *top_of_heap = sbrk(0); // Initial program break
sbrk(1024);                  // Allocate 1 KiB
malloc(16384);               // Allocate 16 KiB
void *top_of_heap2 = sbrk(0);
printf("Heap grew from %p to %p\n", top_of_heap, top_of_heap2);
```

---

**Notes on sbrk**
    • **Program Break:** The end of a process's data segment.
    • Use `malloc`, `calloc`, `realloc` and `free` instead of direct calls to `sbrk`.
    • Security requires zeroing newly allocated memory.

---

### Simple malloc Implementation
```c
void *malloc(size_t size) {
    void *p = sbrk(size);
    if (p == (void *)-1)  // No memory
        return NULL;
    return p;
}
void free() {
    // Does nothing
}
```
>Drawbacks:
    • System calls are slow.
    • No reuse of freed memory.

---
## Placement Strategies for `malloc`

**Options:**
1. **First Fit**: First available hole of sufficient size.

2. **Next Fit**: Continue search from the previous allocation point.

3. **Best Fit**: Smallest hole that can fit memory.

4. **Worst Fit**: Largest hole available for memory.


---


## Fragmentation


**Internal Fragmentation:**

When `malloc` gives more memory than required (e.g., 16 KiB instead
of 2 KiB).
```plaintext

[Allocated 16KB][Internal Waste 14KB]

```

---

**External Fragmentation:**

Enough total memory exists but is split into scattered chunks,
making large allocations (e.g., 32 KiB) impossible.
```plaintext

[Free 8KB][Used 4KB][Free 8KB] → Can't allocate 12KB

```

---


## Challenges of Writing a Heap Allocator


- Minimize fragmentation.
- Maintain high performance.
- Manage pointer arithmetic and coalescing.

---

### Problem:

- Allocation is **application-dependent**.

- Optimal allocation strategy is **NP-hard (Knapsack Problem)**.


---


## Placement Strategy Pros and Cons


**Best Fit**:
- Reduced space wastage in small allocations.
- May return memory of exact/exceeding size.


**Comparisons:**
| Strategy       | Time Complexity | Notes                     |
|----------------|-----------------|---------------------------|
| First Fit      | O(n)            | Simple but may fragment.  |
| Next Fit       | O(1)            | Faster but poorer results |
| Best Fit       | O(n)*           | More efficient memory use |
| Worst Fit      | O(n)*            | Rarely practical.         |

*O(n) can be improved by using max/min heap or similar.
 

---


## Advanced Concepts

 **Segregated Lists**

- Maintain separate lists for different block sizes.
```plaintext
16B | 32B | 64B | 128B | ...  
```
- Efficient allocation and deallocation.

**Buddy Allocator**
- Divide heap blocks into powers of 2.
```plaintext
1024B → 512B/512B → 256B/256B/512B  
```
- Coalesce adjacent free blocks when possible.


---

**Slab Allocation**:  
- Pre-allocated object caches  
- Kernel memory optimization  


**Arena Allocators**:  
- Thread-specific memory pools  
- Lock-free allocation  


**Memory Tagging**:  
- ARM MTE (Memory Tagging Extension)  
- Security-focused allocations  


---


## `sbrk()` vs `mmap()`


| **sbrk**                  | **mmap**                         |
|---------------------------|-----------------------------------|
| No longer POSIX-standard   | POSIX-compliant                  |
| Easy to expand heap         | Handles large blocks efficiently |
| Cannot return memory to OS | Returns memory to OS easily      |


---


## Implementing a Naive Memory Allocator


### Implicit Free List:


```c

typedef struct {
    size_t block_size;
    char data[];
} block;

block *p = sbrk(100);
p->block_size = 100 - sizeof(metadata) - sizeof(BTag);

```

---

### Split & Alignment:

- Aligned to multiples of 16 bytes.
- Avoids unaligned access penalties and crashes.


---


### Coalescing and Free Space


### Explicit Free Lists

- Improved performance by managing free blocks explicitly:


```c

struct block {
    size_t info;  // E.g., in 16-byte units
    struct block *next;
    char data[];
};

```

---


## Summary

1. Navigator strategies: Implicit, explicit, segregated lists.
2. Tactics for fragmentation reduction.
3. Mapped options for large allocations (`mmap` > `sbrk`).
4. Challenges in optimizing allocation algorithms.