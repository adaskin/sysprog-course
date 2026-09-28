
# Lab 1: C Toolchain, Multi-file Projects, Makefiles, Linking, and a Simple Benchmark

**Course:** BIL 322 System Programming  
**Term:** Fall 2026  
**Topic:** Why system programming matters for modern systems
**AI use:** *Prepared with Qwen AI*

---
- [Lab 1: C Toolchain, Multi-file Projects, Makefiles, Linking, and a Simple Benchmark](#lab-1-c-toolchain-multi-file-projects-makefiles-linking-and-a-simple-benchmark)
  - [Lab Objectives](#lab-objectives)
  - [Why This Lab Matters](#why-this-lab-matters)
  - [Part 0: Environment Setup](#part-0-environment-setup)
  - [Part 1: Create a Lab Repository](#part-1-create-a-lab-repository)
  - [Part 2: Multi-file C Project](#part-2-multi-file-c-project)
    - [2.1 Header file: `vector_ops.h`](#21-header-file-vector_opsh)
    - [2.2 Library implementation: `vector_ops.c`](#22-library-implementation-vector_opsc)
    - [2.3 Benchmark program: `bench.c`](#23-benchmark-program-benchc)
  - [Part 3: Compile Manually](#part-3-compile-manually)
    - [3.1 Compile object files](#31-compile-object-files)
    - [3.2 Link the object files](#32-link-the-object-files)
  - [Part 4: Inspect the Compilation Pipeline](#part-4-inspect-the-compilation-pipeline)
    - [4.1 Preprocessing](#41-preprocessing)
    - [4.2 Compile to assembly](#42-compile-to-assembly)
    - [4.3 Assemble to object file](#43-assemble-to-object-file)
    - [4.4 Inspect symbols with `nm`](#44-inspect-symbols-with-nm)
  - [Part 5: Program Sections and Memory Layout](#part-5-program-sections-and-memory-layout)
  - [Part 6: Write a Makefile](#part-6-write-a-makefile)
    - [Makefile Questions](#makefile-questions)
  - [Part 7: Static and Dynamic Linking](#part-7-static-and-dynamic-linking)
    - [7.1 Static library](#71-static-library)
    - [7.2 Shared library](#72-shared-library)
    - [Static vs Shared Library Questions](#static-vs-shared-library-questions)
  - [Part 8: Simple Benchmark](#part-8-simple-benchmark)
    - [8.1 Build without optimization](#81-build-without-optimization)
    - [8.2 Build with optimization](#82-build-with-optimization)
    - [8.3 Compare binary sizes](#83-compare-binary-sizes)
    - [8.4 Benchmark table](#84-benchmark-table)
  - [Optional Part: C vs Python Comparison](#optional-part-c-vs-python-comparison)
  - [Part 9: Git Submission](#part-9-git-submission)
  - [Deliverables](#deliverables)
  - [Grading Checklist](#grading-checklist)
  - [Common Errors and Troubleshooting](#common-errors-and-troubleshooting)
    - [1. `make: *** No rule to make target`](#1-make--no-rule-to-make-target)
    - [2. `undefined reference to dot_product`](#2-undefined-reference-to-dot_product)
    - [3. `error while loading shared libraries: libvector.so`](#3-error-while-loading-shared-libraries-libvectorso)
    - [4. `Permission denied` when running `./bench`](#4-permission-denied-when-running-bench)

## Lab Objectives

By the end of this lab, you should be able to:

1. Set up a Unix/Linux development environment.
2. Create and compile a multi-file C project.
3. Understand the compilation pipeline:
   - preprocessing,
   - compiling,
   - assembling,
   - linking.
4. Use `gcc`, `nm`, `size`, and `ldd` to inspect object files and executables.
5. Write a basic `Makefile`.
6. Understand the difference between object files, static libraries, shared libraries, and executables.
7. Run a simple benchmark and observe the effect of compiler optimization flags.
8. Submit the lab using Git and GitHub Classroom.

---

## Why This Lab Matters

Modern systems such as web servers, databases, operating systems, game engines, vector databases, and AI infrastructure depend on low-level programming concepts:

- object files and linking,
- memory layout,
- system calls,
- process management,
- concurrency,
- file I/O,
- network I/O,
- performance optimization.

Even when AI systems are written in high-level languages like Python, many performance-critical parts are implemented in C, C++, or Rust.

In this lab, we will use a simple **dot-product benchmark**. Dot products are a basic operation in many AI-related systems, such as:

- vector search,
- embeddings,
- recommendation systems,
- similarity search,
- neural-network-style computations.

The goal is not machine learning. The goal is to use a simple, relevant example to learn the C toolchain and system-programming basics.

---

## Part 0: Environment Setup

You need a Linux or Unix-like environment.

Recommended options:

- native Linux,
- macOS,
- WSL2 on Windows,
- Linux virtual machine using VirtualBox or UTM.

On Debian/Ubuntu, install basic tools:

```bash
sudo apt update
sudo apt install build-essential git gdb valgrind
```

Check your compiler:

```bash
gcc --version
make --version
git --version
```

Optional:

```bash
python3 --version
```

Python is not required for this lab, but it may be useful for optional comparison exercises.

---

## Part 1: Create a Lab Repository

Create a directory for the lab:

```bash
mkdir lab1
cd lab1
git init
```

Create the following files:

```text
lab1/
├── vector_ops.h
├── vector_ops.c
├── bench.c
├── Makefile
└── report.md
```

You will fill in these files in the next steps.

---

## Part 2: Multi-file C Project

We will create a small library for vector operations and a benchmark program that uses it.

This is a common structure in real systems:

```text
library code  →  vector_ops.c / vector_ops.h
main program  →  bench.c
```

---

### 2.1 Header file: `vector_ops.h`

Create `vector_ops.h`:

```c
#ifndef VECTOR_OPS_H
#define VECTOR_OPS_H

#include <stddef.h>

double dot_product(const double *a, const double *b, size_t n);

#endif
```

This file declares the interface of our small library.

---

### 2.2 Library implementation: `vector_ops.c`

Create `vector_ops.c`:

```c
#include "vector_ops.h"

double dot_product(const double *a, const double *b, size_t n) {
    double sum = 0.0;

    for (size_t i = 0; i < n; i++) {
        sum += a[i] * b[i];
    }

    return sum;
}
```

This function computes:

```text
a[0] * b[0] + a[1] * b[1] + ... + a[n-1] * b[n-1]
```

This operation appears frequently in similarity search and vector-based systems.

---

### 2.3 Benchmark program: `bench.c`

Create `bench.c`:

```c
#define _POSIX_C_SOURCE 199309L

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "vector_ops.h"

int main(void) {
    const size_t n = 5000000;
    const int runs = 5;

    double *a = malloc(n * sizeof(*a));
    double *b = malloc(n * sizeof(*b));

    if (!a || !b) {
        fprintf(stderr, "malloc failed\n");
        return 1;
    }

    for (size_t i = 0; i < n; i++) {
        a[i] = (double)(i % 1000) / 1000.0;
        b[i] = (double)((i * 3) % 1000) / 1000.0;
    }

    struct timespec start, end;
    double total = 0.0;

    clock_gettime(CLOCK_MONOTONIC, &start);

    for (int r = 0; r < runs; r++) {
        total += dot_product(a, b, n);
    }

    clock_gettime(CLOCK_MONOTONIC, &end);

    double elapsed =
        (double)(end.tv_sec - start.tv_sec) +
        (double)(end.tv_nsec - start.tv_nsec) / 1e9;

    printf("n=%zu runs=%d result=%.6f elapsed=%.6f seconds\n",
           n, runs, total, elapsed);

    free(a);
    free(b);

    return 0;
}
```

This program:

1. Allocates two large arrays.
2. Fills them with simple values.
3. Calls `dot_product` several times.
4. Measures elapsed time.

---

## Part 3: Compile Manually

Before using `make`, compile the project manually to understand the steps.

### 3.1 Compile object files

```bash
gcc -Wall -Wextra -std=c11 -g -c bench.c -o bench.o
gcc -Wall -Wextra -std=c11 -g -c vector_ops.c -o vector_ops.o
```

You should now have:

```bash
ls
```

Expected files:

```text
bench.c
bench.o
vector_ops.c
vector_ops.h
vector_ops.o
```

---

### 3.2 Link the object files

```bash
gcc -Wall -Wextra -std=c11 -g bench.o vector_ops.o -o bench
```

Run:

```bash
./bench
```

Example output:

```text
n=5000000 runs=5 result=8333325.000000 elapsed=0.070000 seconds
```

Your exact numbers may differ.

---

## Part 4: Inspect the Compilation Pipeline

Let us look at what `gcc` is doing internally.

---

### 4.1 Preprocessing

The preprocessor expands `#include` and `#define` directives.

```bash
gcc -E bench.c -o bench.i
```

Look at the size of the preprocessed file:

```bash
wc -l bench.i
```

You will see that `bench.i` is much larger than `bench.c` because the contents of included header files have been expanded.

---

### 4.2 Compile to assembly

```bash
gcc -S -Wall -Wextra -std=c11 -g bench.i -o bench.s
```

Open the assembly file:

```bash
less bench.s
```

You do not need to understand all of it. The point is that C code is translated into assembly before becoming machine code.

---

### 4.3 Assemble to object file

```bash
gcc -c bench.i -o bench_from_preprocessed.o
```

This produces an object file.

---

### 4.4 Inspect symbols with `nm`

Run:

```bash
nm vector_ops.o
```

You should see something like:

```text
0000000000000000 T dot_product
```

The `T` usually means the symbol is defined in the text/code section.

Now inspect `bench.o`:

```bash
nm bench.o
```

You should see something like:

```text
                 U dot_product
```

`U` means undefined. This is expected because `bench.c` calls `dot_product`, but the implementation is in `vector_ops.c`.

After linking:

```bash
nm bench | grep dot_product
```

Now the symbol should be resolved.

---

## Part 5: Program Sections and Memory Layout

Object files and executables contain different sections, such as:

| Section | Meaning |
|---|---|
| `.text` | machine instructions |
| `.data` | initialized global/static variables |
| `.bss` | uninitialized global/static variables |
| `.rodata` | read-only data, such as string constants |

Use the `size` command:

```bash
size bench.o vector_ops.o bench
```

Example style of output:

```text
   text    data     bss     dec     hex filename
   1234       8       0    1242     4da bench.o
    345       0       0     345     159 vector_ops.o
   2048     512       8    2568     a08 bench
```

Your numbers may differ.

You can also inspect sections more verbosely:

```bash
objdump -h bench | head
```

Or:

```bash
readelf -S bench | head
```

For this lab, `size` is enough.

Add the output of `size bench` to your report.

---

## Part 6: Write a Makefile

A `Makefile` automates the build process.

Create a file named `Makefile`.

> Important: Makefile recipe lines must begin with a real TAB character, not spaces.

```make
CC = gcc
CFLAGS ?= -Wall -Wextra -std=c11 -g
LDFLAGS ?=

OBJS = bench.o vector_ops.o
TARGET = bench

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)

%.o: %.c vector_ops.h
	$(CC) $(CFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) *.o *.a *.so *.i *.s

.PHONY: all run clean
```

Build:

```bash
make
```

Run:

```bash
make run
```

Clean:

```bash
make clean
```

---

### Makefile Questions

Answer these briefly in your report:

1. What is the purpose of `CFLAGS`?
2. Why does the `%.o` rule depend on `vector_ops.h`?
3. What happens if you modify `vector_ops.c` and run `make` again?
4. What happens if you modify `vector_ops.h` and run `make` again?

---

## Part 7: Static and Dynamic Linking

We will now create two kinds of libraries from `vector_ops.c`.

---

### 7.1 Static library

Clean previous build artifacts:

```bash
make clean
```

Build the object file:

```bash
gcc -Wall -Wextra -std=c11 -g -c vector_ops.c -o vector_ops.o
```

Create a static library:

```bash
ar rcs libvector.a vector_ops.o
```

Inspect the archive:

```bash
ar t libvector.a
```

Expected output:

```text
vector_ops.o
```

Now link the benchmark against the static library:

```bash
gcc -Wall -Wextra -std=c11 -g -c bench.c -o bench.o
gcc -Wall -Wextra -std=c11 -g bench.o -L. -lvector -o bench_static_lib
```

Run:

```bash
./bench_static_lib
```

Inspect symbols:

```bash
nm bench_static_lib | grep dot_product
```

The `dot_product` code from `libvector.a` has been incorporated into the executable during static linking.

---

### 7.2 Shared library

Build position-independent code:

```bash
gcc -Wall -Wextra -std=c11 -g -fPIC -c vector_ops.c -o vector_ops.pic.o
```

Create a shared library:

```bash
gcc -shared -o libvector.so vector_ops.pic.o
```

Link the benchmark against the shared library:

```bash
gcc -Wall -Wextra -std=c11 -g bench.o -L. -lvector -o bench_shared_lib
```

Run it with:

```bash
LD_LIBRARY_PATH=. ./bench_shared_lib
```

Check dynamic dependencies:

```bash
LD_LIBRARY_PATH=. ldd bench_shared_lib
```

You should see system libraries such as `libc.so.6`, and possibly your local `libvector.so`.

Inspect exported dynamic symbols:

```bash
nm -D libvector.so | grep dot_product
```

---

### Static vs Shared Library Questions

Answer briefly in your report:

1. What file extension is used for static libraries on Linux?
2. What file extension is used for shared libraries on Linux?
3. What does `ldd` show?
4. What is one advantage of shared libraries?
5. What is one advantage of static libraries?

---

## Part 8: Simple Benchmark

Now we will benchmark the same program with different optimization levels.

---

### 8.1 Build without optimization

```bash
make clean
make CFLAGS="-Wall -Wextra -std=c11 -O0"
mv bench bench_O0
```

Run:

```bash
./bench_O0
```

Record the elapsed time.

---

### 8.2 Build with optimization

```bash
make clean
make CFLAGS="-Wall -Wextra -std=c11 -O2"
mv bench bench_O2
```

Run:

```bash
./bench_O2
```

Record the elapsed time.

---

### 8.3 Compare binary sizes

```bash
ls -lh bench_O0 bench_O2
```

---

### 8.4 Benchmark table

Fill in a table like this in your report:

| Build | Optimization | Elapsed time | Binary size |
|---|---:|---:|---:|
| `bench_O0` | `-O0` | | |
| `bench_O2` | `-O2` | | |

Answer:

1. Which version was faster?
2. Why does `-O2` usually produce faster code?
3. Does faster execution come with any possible tradeoffs?

---

## Optional Part: C vs Python Comparison

This part is optional.

If you have Python installed, create `bench.py`:

```python
import time

n = 1_000_000
runs = 5

a = [(i % 1000) / 1000.0 for i in range(n)]
b = [((i * 3) % 1000) / 1000.0 for i in range(n)]

start = time.perf_counter()

total = 0.0
for _ in range(runs):
    s = 0.0
    for i in range(n):
        s += a[i] * b[i]
    total += s

end = time.perf_counter()

print(f"n={n} runs={runs} result={total:.6f} elapsed={end - start:.6f} seconds")
```

Run:

```bash
python3 bench.py
```

Compare with the C benchmark.

Important: use a smaller `n` for Python, otherwise it may be very slow.

Answer briefly:

1. Which version was faster?
2. Why is Python often slower for this kind of numeric loop?
3. Why do many AI/ML systems still use Python even when performance-critical parts are implemented in C/C++?

---

## Part 9: Git Submission

Create `report.md` and include:

1. Your name and student ID.
2. The output of:

```bash
gcc --version
```

3. The output of:

```bash
nm vector_ops.o
nm bench.o
```

4. The output of:

```bash
size bench
```

5. The static/shared library commands you used and their outputs.
6. Your benchmark table.
7. Short answers to the questions in the lab.

Add and commit:

```bash
git add vector_ops.h vector_ops.c bench.c Makefile report.md
git commit -m "Lab 1: C toolchain, Makefile, linking, benchmark"
```

Push to your GitHub Classroom repository:

```bash
git push origin main
```

If your default branch is `master`, use:

```bash
git push origin master
```

---

## Deliverables

Submit the following in your repository:

```text
vector_ops.h
vector_ops.c
bench.c
Makefile
report.md
```

Your report should contain:

- command outputs,
- benchmark table,
- short answers,
- one short paragraph explaining why system programming matters for modern systems.

---

## Grading Checklist

| Item | Points |
|---|---:|
| Working multi-file C project | 20 |
| Correct Makefile | 20 |
| Manual compilation and symbol inspection | 20 |
| Static/shared library comparison | 20 |
| Benchmark table and short answers | 20 |

Total: **100**

---

## Common Errors and Troubleshooting

### 1. `make: *** No rule to make target`

Check that your Makefile recipe lines start with TABs, not spaces.

---

### 2. `undefined reference to dot_product`

You probably forgot to link `vector_ops.o` or the vector library.

Example of incorrect linking:

```bash
gcc bench.o -o bench
```

Correct:

```bash
gcc bench.o vector_ops.o -o bench
```

or:

```bash
gcc bench.o -L. -lvector -o bench
```

---

### 3. `error while loading shared libraries: libvector.so`

The dynamic linker cannot find your shared library.

Run with:

```bash
LD_LIBRARY_PATH=. ./bench_shared_lib
```

---

### 4. `Permission denied` when running `./bench`

Make sure the executable exists and has execute permission:

```bash
chmod +x bench
```

Usually `gcc` already creates an executable file.

---
