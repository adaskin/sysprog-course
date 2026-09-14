---
title: "System Programming - Introduction"
author: "Ammar Daskin"
institute: "Istanbul Medeniyet University, Computer Engineering Department"
marp: true
theme: default
paginate: true
size: 16:10
class: invert
---


<style type="text/css">
div {
  font-size: clamp(10px, 2vw, 32px);

</style> 

# 🎓 BIL 322 System Programming - Spring 2026

Instructor: Ammar Daskin  
📍 Lectures: Monday @ 501, 12:30  
🧪 Labs: Thursday @ BIL-Lab, 11:30  

This course is a system-programming course focused on Unix/Linux programming using **C** as the primary language. Selected short Rust examples are used only to compare concepts such as memory safety, error handling, and concurrency.

---

# 🕒 Course Hours

- **Lectures:** Monday @ 501, 12:30
  - Slides and examples are posted on Google Classroom / course repository.
  - Lectures include live coding, mostly in C.

- **Labs:** Thursday @ BIL-Lab, 11:30
  - Labs/quizzes via GitHub Classroom and/or Google Classroom.
  - Labs practice the concepts covered in lectures.

- **Projects:**
  - Individual and group programming assignments.
  - Some projects may involve threads, IPC, sockets, processes, and optional GUI visualization.

---

# ✅ Prerequisites

Mandatory:

- Passing grade in **BIL 115** or an equivalent introductory C programming course.
- Basic understanding of data structures and algorithms.

Recommended:

- Basic computer architecture fundamentals.
- Some familiarity with Linux command line.
- Basic Git/GitHub knowledge is useful but not required.

**No prior Rust experience is assumed.**  
Rust is used only for short conceptual comparisons with C.

---


# 🧭 Course Language Policy



## Primary language: C

C is the main language of the course because it gives direct access to:

- system calls,
- file descriptors,
- processes,
- memory layout,
- threads,
- synchronization,
- IPC,
- signals,
- sockets.

---

## Rust: short conceptual examples only

Rust may appear in small examples such as:

- ownership vs. `malloc`/`free`,
- `Result` vs. `errno`,
- `std::process::Command` vs. `fork`/`exec`,
- `Arc<Mutex<T>>` vs. `pthread_mutex_t`.

This is **not** a full Rust course.

Advanced Rust topics such as `tokio`, async, custom Rust allocators, deep lifetimes, and extensive `unsafe` Rust are not required.


---

# 📚 Resources & Textbooks

| Type | Resources |
| --- | --- |
| Core Textbook | [System Programming Wiki](https://github.com/angrave/SystemProgramming/wiki) |
| Linux Guide | [The Linux Programming Interface](https://man7.org/index.html) by Kerrisk |
| C Programming | *The C Programming Language* by Kernighan & Ritchie |
| Advanced | *Linux System Programming* by Love, *Advanced UNIX Programming* by Stevens |
| Optional Rust | [The Rust Programming Language](https://doc.rust-lang.org/book/) |
| Optional Rust | [Rust by Example](https://doc.rust-lang.org/rust-by-example/) |

No required textbook. Lecture notes are posted weekly.

---

# 📊 Grading Breakdown

| Category | Weight | Details |
| --- | ---: | --- |
| 🧪 Weekly Labs/Quizzes | 10% | Lowest grade dropped |
| 📝 Programming Assignments | 30% | 3–4 assignments, including group project |
| 📘 Midterm Exam | 20% | Theory and implementation |
| 📚 Final Exam | 40% | Comprehensive assessment |

Letter grades are determined according to department policy.

---

# 🚀 How to Succeed in This Course

## Lectures

- Focus on theory and simple implementations.
- Take notes — some details may not be in the slides.
- Many examples are done live in class.

## Labs

- Practice the concepts from lectures.
- Labs may include debugging, short coding tasks, and quizzes.
- Some labs may include bonus topics.

---

## Projects

- Implement system programs such as shells, file utilities, allocators, threaded applications, or socket-based systems.
- Submit via GitHub Classroom and/or Google Classroom.
- Some group projects may include optional GUI visualization using GTK, Qt, or SDL, but the core system logic is expected to be C/POSIX-based.

---

# 💻 What is System Programming?

> Low-level software that interfaces directly with the OS kernel and core system libraries.

Examples:

- Text editors: `vim`, `emacs`
- Debuggers: `gdb`, `valgrind`
- Network servers: `nginx`, `Apache`
- Shells: `bash`, `zsh`
- Compilers and linkers
- Database storage engines
- Runtime systems and allocators

---

# 🌟 Why Learn System Programming?

- Write efficient low-level code.
- Understand what happens below high-level languages.
- Learn how programs interact with the operating system.
- Build tools such as shells, servers, allocators, and debuggers.
- Prepare for advanced courses such as:
  - operating systems,
  - computer architecture,
  - compilers,
  - distributed systems,
  - high-performance computing.

---

# 🐧 Unix System Programming

Key components:

```text
User Program → System Call → Kernel → Hardware
```

---

Important tools and concepts:

- `gcc` compiler
- `make`
- Git
- GDB
- Valgrind
- system calls for:
  - file I/O,
  - process control,
  - memory management,
  - IPC,
  - signals,
  - networking.

---

# 📲 Portability of Programs

## API

Application Programming Interface:

- source-code-level interface,
- example: POSIX functions such as `fork`, `open`, `read`, `write`.

---

## ABI

Application Binary Interface:

- compiled-program-level interface,
- calling conventions,
- object-file format,
- system-call interface.

A program compiled for one ABI may not run on another ABI without recompilation.

---

# 📜 Compliance & Standards

Important standards:

- **POSIX:** improves cross-Unix compatibility.
- **ISO C:** standard C language features.
- Linux man pages:
  - Section 2: system calls,
  - Section 3: library functions.

Examples:

```bash
man 2 open
man 3 printf
```

---

# 📅 Weekly Topics

Small changes may occur during the semester.

1. Course administrivia, Git, GCC, Makefiles, C crash course
2. Macros, pointers, function pointers, memory model
3. Structs, alignment, binary file I/O, debugging
4. System calls, file descriptors, Unix file I/O
5. Processes: `fork`, `exec`, `wait`, simple shell
6. Memory allocators
7. Introduction to threads and parallel computation
8. Midterm

---

9.  Threads II, thread pools, synchronization basics
10. Mutexes, condition variables, semaphores
11. Deadlock, reader/writer, producer/consumer examples
12. Virtual memory and IPC: pipes, `mmap`, shared memory
13. Networking and socket programming I
14. Signals, socket programming II, filesystems
15. Final review and project demos

Short Rust comparison examples may appear in some weeks, but they are not separate major topics.

---

# This Week’s Topics

- Version control basics
- Git fundamentals
- Crash course on C
- Program files
- Compilation steps
- Makefile essentials
- Optional: very short Rust toolchain demo, `rustc`/`cargo`

---

# What is Version Control?

A system to:

- track changes to code/files over time,
- collaborate without overwriting work,
- revert to previous states.

Key terms:

- **Repository:** database of project history
- **Commit:** snapshot of changes
- **Branch:** parallel development line
- **Merge:** combine changes from branches

---

# Git Basics

Distributed version control:

- local and remote repositories,
- every clone has full history.

---

Essential commands:

```bash
git init                 # Create new repo
git add [file]           # Stage changes
git commit -m "message"  # Save snapshot
git branch               # List/create branches
git checkout             # Switch branches
git merge                # Combine branches
git push                 # Upload changes
git pull                 # Download changes
```

---

# Typical Git Workflow

```text
          commit
Work Dir ----> Staging Area ----> Repository
             (git add)         (git commit)
```

Branching model:

```text
main ────┐
          ├─ merge ──> deploy
dev  ────┘
```

---

# 🌿 Git Essentials

Basic workflow:

```bash
# Initialize and commit
git init
git add *.c LICENSE
git commit -m "Initial commit"

# Sync with GitHub
git remote add origin https://github.com/user/repo.git
git push -u origin main
```

---

Tips:

- Use SSH keys for secure authentication.
- Use meaningful commit messages.
- Do not commit compiled binaries unless explicitly requested.

---

# Accepting GitHub Classroom Assignments

1. Use the assignment link provided in class or Google Classroom.
2. Open the link in your browser.
3. Choose your identifier to link your GitHub account.
4. Accept the assignment.
5. GitHub Classroom creates a private repository for you.
6. Clone the repository and start working.

---

# Submitting Your Assignment

```bash
git clone https://github.com/YOUR-USERNAME/YOUR-REPOSITORY
cd YOUR-REPOSITORY

# Make some changes or add files
echo "int main(){}" > newfile.c

git add newfile.c
git commit -m "new file added"
git push origin main
```

After pushing, your assignment is submitted to the remote repository.

If required, also submit the repository link on Google Classroom.

---

# Crash Course on C and Development Tools

C examples done in class are posted in the lecture notes.

A program file contains the information needed to load an application into memory and run it, including:

- machine instructions,
- initialized data,
- list of library dependencies,
- list of memory sections,
- symbols that are resolved during linking or loading.

---

# Example with C

```bash
cat > main.c
```

```c
#include <stdio.h>

int main() {
    printf("Hello World!\n");
    return 0;
}
```

Compile and run:

```bash
gcc main.c
./a.out
```

---

# ⚙️ Compilation Steps

```bash
gcc -E main.c -o main.i   # Preprocess
gcc -S main.i -o main.s   # Compile to assembly
gcc -c main.s -o main.o   # Assemble object file
gcc main.o -o main        # Link executable
```

Run:

```bash
./main
```

---

# 🔧 GCC Compilation Deep Dive

```bash
# Step 1: Preprocess
gcc -E main.c > main.i

# Step 2: Generate assembly
gcc -S main.c

# Step 3: Assemble to object file
gcc -c main.c

# Inspect symbols
nm main.o

# Link object files
gcc -o main main.o

# Check shared libraries
ldd main
```

---

# Program Loading Explained

The loader:

- allocates memory for `.text`, `.data`, `.bss`,
- loads shared libraries if needed,
- resolves symbols,
- starts execution.

Execution usually starts from:

```text
_start → initialization → main() → exit()
```

Check shared-library dependencies:

```bash
ldd a.out
```

---

# 📦 Static vs. Shared Libraries

| Feature | Static Libraries `.a` | Shared Libraries `.so` |
| --- | --- | --- |
| Size impact | Larger executable | Smaller executable |
| Loading | Linked at compile time | Loaded at runtime |
| Memory usage | Per-process copy | Shared system-wide instance |
| Linking | Static linker | Dynamic linker |

Examples:

- Static: `libmath.a`
- Shared: `libc.so`

---

# 🛠️ Makefiles 101

Makefiles automate build processes.

They:

- define build rules,
- manage dependencies,
- avoid repetitive commands,
- rebuild only changed files.

Basic syntax:

```make
target: dependencies
    commands
```

---

# Simple Makefile Example

```make
CC = gcc
CFLAGS = -Wall -g

build: main.c aux.c
	$(CC) $(CFLAGS) -o program main.c aux.c

clean:
	rm -f program *.o
```

Run:

```bash
make build
make clean
```

---

# Why Use Makefiles?

- Automation: build with one command.
- Efficiency: rebuild only what changed.
- Standardization: same build process for everyone.
- Flexibility: build, test, clean, deploy.

Useful references:

- [Makefile Tutorial](https://makefiletutorial.com/)
- [GNU Make Manual](https://www.gnu.org/software/make/manual/make.html)

---

# Beyond Make: Build System Alternatives

| Language | Tools | Use Case |
| --- | --- | --- |
| C/C++ | Make, CMake, Ninja | General systems projects |
| Java | Maven, Gradle, Ant | Dependency management |
| Rust | Cargo | Rust projects |
| Go | `go build` | Simple Go builds |

In this course, we mainly use:

- `gcc`
- `make`
- Git

Cargo may appear only in very small Rust comparison examples.

---

# Optional Rust Corner: Toolchain Comparison

This is only a short comparison, not a full Rust lesson.

C:

```bash
gcc main.c -o main
./main
```

Rust:

```bash
rustc main.rs
./main
```

Or with Cargo:

```bash
cargo new hello
cd hello
cargo run
```

Main point:

Both C and Rust can produce native binaries, but this course uses C for the main system-programming topics.

---

# 🧪 This Week’s Lab: Git & C

Tasks:

1. Create a multi-file C project.
2. Write a simple `Makefile`.
3. Initialize a Git repository.
4. Commit your files.
5. Push to GitHub Classroom.
6. Submit the assignment.

Optional:

- Install Rust with `rustup`.
- Compile a hello-world program using `rustc` or `cargo`.

---

# ➡️ Next Lecture Preview

Next week:

- Pointers
- Pointer arithmetic
- Function pointers
- Structs
- Memory layout
- Alignment and padding
- Binary file I/O
- GDB and Valgrind
- Short Rust comparison: references and `Box`