# BIL 322 System Programming  
**Fall 2026, Department of Computer Engineering, Istanbul Medeniyet University**

- [2026 lecture notes](lectures/)  
- [2025 BIL 222 notes](2025-bil222-lecture-notes/)  
- [2024 BIL 222 notes](https://sites.google.com/view/adaskin/system-programming)

---

## Meeting Times

- **Lectures:** Monday @ 501, 12:30  
- **Labs:** Thursday @ BIL-Lab, 11:30  

---

## Prerequisites

Students are expected to have prior C programming experience and basic knowledge of data structures and algorithms.

A passing grade in **BIL 115** is required.  
The old equivalent C programming course, such as **BIL 121**, may be considered equivalent.

**No prior Rust experience is required.**  
Rust appears only through short conceptual examples comparing it with C.

---

## Course Overview

> This course introduces Unix/Linux system programming using C as the primary language. Topics include the C toolchain, object files, pointers, memory layout, system calls, file I/O, processes, memory allocators, threads, synchronization, IPC, networking, signals, and filesystems. Selected Rust examples are used to compare modern approaches to memory safety, error handling, and concurrency. The course includes labs and a group project, which may involve sockets, IPC, threads, and optional GUI visualization.
 
The course is primarily taught in **C**, because C gives direct access to system calls, memory layout, file descriptors, processes, threads, synchronization, IPC, signals, and sockets.

The first part of the course covers advanced C topics and the C toolchain:

- `gcc`, `make`, Git (version control), GDB, Valgrind,
- macros, pointers, function pointers,
- structs, alignment, padding,
- object files and linking,
- binary file I/O.

The second part covers Unix system programming:

- system calls and file I/O,
- processes, `fork`, `exec`, `wait`,
- simple shell implementation,
- memory allocators,
- threads and synchronization,
- deadlocks, producer/consumer, reader/writer,
- virtual memory and IPC,
- pipes, shared memory, message queues,
- networking and socket programming,
- signals,
- filesystems.

Selected **Rust examples** are used briefly during the semester to compare modern safety mechanisms with C. These examples are conceptual and short. They are not intended to turn the course into a full Rust course.

Some labs and the group project may include visualization or GUI components using libraries such as **GTK**, **Qt**, or **SDL**, but the core system-programming logic is expected to be implemented using C and POSIX APIs.

---

## Course Language Policy

### Primary language: C

Most lectures, labs, assignments, and projects are in C.

### Rust: short conceptual comparisons only

Rust is used only for small examples such as:

- ownership versus manual `malloc`/`free`,
- `Box`/`Vec` versus heap allocation,
- `Result` versus `errno`,
- `std::process::Command` versus `fork`/`exec`,
- `Arc<Mutex<T>>` versus `pthread_mutex_t`.

Students are expected to read and understand simple Rust snippets, but they are not expected to write large Rust programs.

Advanced Rust topics such as `tokio`, async, custom Rust allocators, deep lifetimes, and extensive `unsafe` Rust are not required.

### Other languages

Socket programming may briefly be compared with Python or Java examples, but C remains the main implementation language.

---

## Learning Outcomes

By the end of the course, students should be able to:

1. Use C development tools such as `gcc`, `make`, Git, GDB, and Valgrind.
2. Understand object files, linking, symbols, and program loading.
3. Understand C memory layout, pointers, structs, alignment, and binary file I/O.
4. Use Unix system calls for file I/O and process control.
5. Implement a simple shell using `fork`, `exec`, `wait`, pipes, and redirection.
6. Implement a simple memory allocator.
7. Write multithreaded C programs using POSIX threads.
8. Use synchronization primitives such as mutexes, condition variables, and semaphores.
9. Understand race conditions, deadlock, livelock, and common concurrency bugs.
10. Understand IPC mechanisms such as pipes, shared memory, `mmap`, and message queues.
11. Write basic TCP/UDP socket programs in C.
12. Understand signals and basic asynchronous event handling.
13. Compare C with Rust conceptually in terms of memory safety, error handling, and concurrency.
14. Work on a substantial system-programming project, possibly involving threads, IPC, sockets, and optional GUI visualization.

---

## Textbooks and Course Material

No required textbook.

Lecture notes are posted weekly and are based on previous course materials and the following resources:

- [System Programming Wiki, Lawrence Angrave](https://github.com/angrave/SystemProgramming/wiki)
- [CS 341 Coursebook, UIUC](https://cs341.cs.illinois.edu/coursebook/index.html)
- [The Linux Programming Interface, Michael Kerrisk](https://man7.org/tlpi/index.html)
- [man7.org Linux manual pages](https://man7.org/index.html)
- *The C Programming Language*, Kernighan and Ritchie

Optional Rust references:

- [The Rust Programming Language](https://doc.rust-lang.org/book/)
- [Rust by Example](https://doc.rust-lang.org/rust-by-example/)

---

## Weekly Content

Small changes may occur during the semester.

| Week | Topics | Rust / Comparison Corner |
|---|---|---|
| 1 | Course intro, C crash course, `gcc`, Makefiles, Git, object files | `rustc`/`cargo` hello-world |
| 2 | Macros, pointers, function pointers, structs, alignment, binary file I/O | Rust references, `Box`, `#[repr(C)]` |
| 3 | System calls, file descriptors, Unix file I/O, `stat`, `lseek` | Rust `Result` vs C `errno` |
| 4 | Processes: `fork`, `exec`, `wait`, zombies/orphans, simple shell | Rust `Command` vs `fork`/`exec` |
| 5 | Memory allocators, `malloc`/`free`, `sbrk`, alignment, fragmentation | Rust `Box`/`Vec` and automatic drop |
| 6 | Intro to threads, POSIX threads, race conditions | Rust `std::thread` |
| 7 | Thread pools, parallelism, coroutines/green threads conceptually | Rust threads conceptually |
| 8 | Midterm exam | — |
| 9 | Synchronization: mutexes, condition variables, semaphores | Rust `Arc<Mutex<T>>` |
| 10 | Producer/consumer, reader/writer, deadlock, dining philosophers | Rust compile-time race prevention conceptually |
| 11 | Virtual memory, IPC: pipes, FIFOs, `mmap`, shared memory, message queues | Rust channels conceptually |
| 12 | Networking intro: TCP/UDP, HTTP conceptually, socket programming I | Optional Python/Java socket comparison |
| 13 | Socket programming II, client/server design, project networking phase | — |
| 14 | Signals, signal handlers, `sigaction`, filesystems | — |
| 15 | Final review and project demos | — |

---

## Labs and Quizzes

Labs and quizzes are assigned weekly or near-weekly through Google Classroom and/or GitHub Classroom.

They are generally short exercises related to the current topic.

The lowest lab/quiz grade is dropped.

Examples of lab topics from previous semesters include:

- Git, Makefiles, and C project setup
- debugging with GDB and Valgrind
- LSB steganography using binary file I/O
- simple shell implementation
- pipes and redirection
- memory allocator implementation
- multithreaded game or simulation
- thread pools
- socket-based project phase
- signals and asynchronous I/O

Some labs may include small visualization components, but the main graded part is system programming in C.

---

## Homework / Coding Assignments

There will be approximately **3–4 programming assignments** which can be done as **group projects**.

Assignments are submitted through Google Classroom and/or GitHub Classroom.

No late submissions are accepted unless officially approved.

The projects generally involves substantial system-programming concepts. Possible assignment themes include:

- file I/O and binary processing,
- process management and shell,
- pipes/redirection,
- memory allocator,
- multithreaded application,
- socket-based client/server system.

Examples of possible projects include:

- emergency drone coordination system,
- communicating shells,
- concurrent simulation or game backend,
- client/server monitoring system,
- distributed task coordinator.

Students may use GUI libraries such as **GTK**, **Qt**, or **SDL** for visualization, but the GUI is not the main focus. The core system logic should demonstrate C/POSIX system-programming concepts.

A short project demo may be done during the final week.

---

## Grading

| Category | Weight |
|---|---:|
| Labs / Quizzes | 10% |
| Midterm Exam | 20% |
| Programming Assignments / Project | 30% |
| Final Exam | 40% |

The lowest lab/quiz grade is dropped.

---

## Course Sites

- Assignment submission and grading: Google Classroom and/or GitHub Classroom.
- Discussion and Q&A: Piazza.
- Lecture notes: posted weekly.

In discussions:

- Do not post solutions or significant parts of assignments.
- Do not post unrelated content.
- Ask questions when you need help.
- Post helpful answers when you can help others.

---

## Environment Settings

You should have a Linux installation or Unix-like environment.

Recommended options:

- native Linux,
- macOS,
- WSL2 on Windows,
- Linux virtual machine using VirtualBox, UTM, or similar.

Basic tools:

```bash
sudo apt update
sudo apt install build-essential git gdb valgrind
```

Optional Rust installation:

```bash
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
```

Optional GUI libraries, depending on the project:

```bash
sudo apt install libgtk-3-dev
# or Qt development packages
# or SDL2 development packages
```

Rust and GUI libraries are optional for the core course material. Students should not be blocked by GUI or Rust toolchain problems.

---

## Collaboration and Cheating Policy

Any kind of plagiarism or cheating is prohibited. Please refer to the university policy.

You may discuss assignments and projects with classmates, but all submitted work must be your own.

If you benefit from the work of others, list them or the referenced materials in your submission.

You must be able to explain every significant part of your submitted code.

---

## Use of AI Tools

You may use AI tools such as Qwen, GPT, Gemini, DeepSeek, Claude, and similar systems for explanation, debugging, and generating study examples.

However, over-reliance may prevent you from developing real problem-solving skills.

The golden rule is:

> You must be able to explain every line of code you submit, and you must have written and understood the core logic yourself.

If you use AI assistance, briefly list it in your submission.

Example:

> I used an AI assistant to understand Valgrind output and to generate test cases. The code was written by me.

