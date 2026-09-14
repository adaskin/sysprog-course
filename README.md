# BIL 322 System Programming
**Fall 2026, Department of Computer Engineering, Istanbul Medeniyet University**



- [2026 lecture notes](lectures/)
- [2025 BIL 222 notes](2025-bil222-lecture-notes/)
- [2024 BIL 222 notes](https://sites.google.com/view/adaskin/system-programming)

*This document and lecture notes have been rewritten and prepared with the help of different AI tools: mostly Qwen and DeepSeek*

---

## Meeting Times

- **Lectures:** Monday @ 501, 12:30
- **Labs:** Thursday @ BIL-Lab, 11:30

---

## Prerequisites

Students are expected to have prior C programming experience and basic knowledge of data structures and algorithms.

A passing grade in **BIL 115** is required.  
The old equivalent C programming course, such as **BIL 121**, may be considered equivalent.

No prior Rust experience is required. Rust may appear only occasionally through short conceptual examples.

---

## Course Overview

This course introduces Unix/Linux system programming using **C** as the primary language.

The course covers the core mechanisms that modern software systems depend on:

- C toolchain, object files, linking,
- pointers, memory layout, structs, alignment,
- system calls and file I/O,
- processes, `fork`, `exec`, `wait`,
- pipes, redirection, shells,
- memory allocators,
- threads and synchronization,
- deadlock and concurrency bugs,
- virtual memory and IPC,
- networking and sockets,
- signals,
- filesystems.

Many examples and project ideas are motivated by modern systems use cases, especially **AI infrastructure**, such as:

- data pipelines,
- worker processes,
- request queues,
- thread pools,
- batching systems,
- model-like worker servers,
- vector search services,
- concurrent client/server systems.

This is still a system-programming course, not a machine-learning course. The goal is to understand and build the low-level systems that applications such as AI services, servers, data pipelines, and concurrent tools rely on.

Selected Rust examples may be used briefly during the semester to compare memory safety, error handling, or concurrency ideas. These examples are optional, conceptual, and not a major part of the course.

---

## Course Language Policy

### Primary language: C

Most lectures, labs, assignments, and projects are in C.

C is used because it gives direct access to:

- system calls,
- file descriptors,
- processes,
- memory layout,
- threads,
- synchronization,
- IPC,
- signals,
- sockets.

### Rust: occasional conceptual examples only

Rust may appear in very short examples such as:

- ownership versus manual `malloc`/`free`,
- `Result` versus `errno`,
- `std::process::Command` versus `fork`/`exec`,
- `Arc<Mutex<T>>` versus `pthread_mutex_t`.

Students are not expected to write large Rust programs.

Advanced Rust topics such as `tokio`, async, custom Rust allocators, deep lifetimes, and extensive `unsafe` Rust are not required.

### Other languages

Python or Java examples may occasionally be used for comparison, especially in networking or high-level API discussions, but C remains the main implementation language.

---

## Learning Outcomes

By the end of the course, students should be able to:

1. Use C development tools such as `gcc`, `make`, Git, GDB, and Valgrind.
2. Understand object files, linking, symbols, and program loading.
3. Understand C memory layout, pointers, structs, alignment, and binary file I/O.
4. Use Unix system calls for file I/O and process control.
5. Implement simple shells or pipeline runners using `fork`, `exec`, `wait`, pipes, and redirection.
6. Implement a simple memory allocator.
7. Write multithreaded C programs using POSIX threads.
8. Use synchronization primitives such as mutexes, condition variables, and semaphores.
9. Understand race conditions, deadlock, livelock, and common concurrency bugs.
10. Understand IPC mechanisms such as pipes, shared memory, `mmap`, and message queues.
11. Write basic TCP/UDP socket programs in C.
12. Understand signals and basic asynchronous event handling.
13. Apply system-programming concepts to modern application domains such as data pipelines, worker systems, request servers, and simple AI-infrastructure-style projects.
14. Work on a substantial system-programming project, possibly involving threads, IPC, sockets, signals, and optional GUI visualization.

---

## Textbooks and Course Material

No required textbook.

Lecture notes are posted weekly and are based on previous course materials and the following resources:

- [System Programming Wiki, Lawrence Angrave](https://github.com/angrave/SystemProgramming/wiki)
- [CS 341 Coursebook, UIUC](https://cs341.cs.illinois.edu/coursebook/index.html)
- [The Linux Programming Interface, Michael Kerrisk](https://man7.org/tlpi/index.html)
- [man7.org Linux manual pages](https://man7.org/index.html)
- *The C Programming Language*, Kernighan and Ritchie

Optional Rust references, only if short Rust examples are used:

- [The Rust Programming Language](https://doc.rust-lang.org/book/)
- [Rust by Example](https://doc.rust-lang.org/rust-by-example/)

---

## Weekly Content

Small changes may occur during the semester.

Short Rust comparisons may appear occasionally when useful, but Rust is not a weekly topic and is not a major part of the course.

| Week | Core System Programming Topic | Example / Application Focus |
|---|---|---|
| 1 | Course intro, C toolchain, Git, Makefiles, object files | Why system programming matters for modern systems; simple C benchmark example |
| 2 | Pointers, memory model, structs, alignment, binary file I/O | Tensor-like buffers, binary layouts, file formats |
| 3 | Debugging, GDB, Valgrind, Unix file I/O, system calls | Dataset/log/checkpoint file processing |
| 4 | Processes: `fork`, `exec`, `wait`, zombies/orphans | Worker processes and process supervision |
| 5 | Pipes, redirection, shell/pipeline implementation | Simple data pipeline: filter / transform / process |
| 6 | Memory allocators | Tensor arena or memory-pool style allocator |
| 7 | Threads introduction, POSIX threads, race conditions | Parallel vector/embedding-like computation |
| 8 | Midterm exam | — |
| 9 | Synchronization: mutexes, condition variables, semaphores | Thread-safe queues and shared buffers |
| 10 | Thread pools, producer/consumer, request queues | Request batching or task scheduling |
| 11 | Deadlock, livelock, reader/writer, dining philosophers | Resource contention in concurrent servers/workers |
| 12 | Virtual memory and IPC: pipes, FIFOs, `mmap`, shared memory | Shared data loaders or shared embedding/index files |
| 13 | Networking intro, TCP/UDP, socket programming I | Simple request server or vector-search style service |
| 14 | Signals, socket programming II, filesystems | Graceful shutdown, health checks, file-based caching |
| 15 | Final review and project demos | Student project presentations/demos |

---

## Labs and Quizzes

Labs and quizzes are assigned weekly or near-weekly through Google Classroom and/or GitHub Classroom.

They are generally short exercises related to the current topic.

The lowest lab/quiz grade is dropped.

Possible lab topics include:

- Git, Makefiles, and multi-file C project setup
- C performance benchmarking and debugging
- GDB/Valgrind debugging exercises
- binary file I/O with dataset/tensor-like files
- simple shell or pipeline runner
- memory allocator or memory-pool implementation
- parallel computation with pthreads
- thread-safe queue or request buffer
- thread pool or task scheduler
- IPC worker system
- socket-based client/server exercise
- signals and graceful shutdown

Some labs may include small visualization components, but the main graded part is system programming in C.

---

## Homework / Coding Assignments

There will be approximately **3–4 programming assignments**, which can be done as group projects.

Assignments are submitted through Google Classroom and/or GitHub Classroom.

No late submissions are accepted unless officially approved.

Assignment and project themes may include:

- file I/O and binary processing,
- process management and pipeline execution,
- pipes/redirection,
- memory allocator,
- multithreaded application,
- request queue or thread pool,
- socket-based client/server system.

Example project ideas include:

- mini inference-like request server,
- vector search engine,
- dataset preprocessing pipeline,
- multi-agent or drone coordination system,
- communicating shells,
- model/process supervisor,
- concurrent simulation or game backend,
- client/server monitoring system.

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

Optional Rust installation, only if you want to try the short Rust examples:

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
