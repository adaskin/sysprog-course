# BIL 322 System Programming (Spring 2026, Dept of Computer Engineering, Istanbul Medeniyet Unv.)
------------------------------------------------------------------------------
* [2026 lecture notes](lectures/)
* [2025 BIL222 notes](2025-bil22-lecture-notes)
* [2024 BIL222 notes](https://sites.google.com/view/adaskin/system-programming) 

---

- [BIL 322 System Programming (Spring 2026, Dept of Computer Engineering, Istanbul Medeniyet Unv.)](#bil-322-system-programming-spring-2026-dept-of-computer-engineering-istanbul-medeniyet-unv)
  - [Meeting Times](#meeting-times)
  - [Prerequisites](#prerequisites)
  - [Overview](#overview)
  - [Learning Outcomes](#learning-outcomes)
  - [Textbooks and Course Material](#textbooks-and-course-material)
  - [Weekly Content](#weekly-content)
  - [Quizzes and labs](#quizzes-and-labs)
  - [Homework-coding assignments](#homework-coding-assignments)
  - [Grading](#grading)
  - [Course sites](#course-sites)
  - [Environment Settings](#environment-settings)
  - [Collaboration and Cheating Policy](#collaboration-and-cheating-policy)
  - [Use of AI, GPT, Gemini, DeepSeek, etc.](#use-of-ai-gpt-gemini-deepseek-etc)
    - [Example Uses and Prompts](#example-uses-and-prompts)

## Meeting Times
- Labs on Thursday @BIL-Lab 11:30am,
- Lectures on Monday @501 at 12:30pm

## Prerequisites
You are expected to have prior C programming experience and a basic understanding of data structures and algorithms.  
A passing grade in BIL 115 is required. **No prior Rust experience is assumed** – Rust will be taught alongside C during the semester.

## Overview
This course introduces the fundamentals of system programming in Unix-like environments, with a **dual emphasis on C and Rust**.  
In the first part we cover low‑level execution mechanics, memory layout, and custom allocators – first in C, then re‑expressed through Rust’s ownership and smart pointers.  
The second part focuses on concurrency, inter‑process communication, and asynchronous I/O, using both POSIX APIs (pthreads, `epoll`, shared memory) and Rust’s safe concurrency primitives (`std::thread`, `Arc<Mutex<T>>`, `tokio`).  

By working in two languages, students learn not only *how* the OS interface works, but also *why* modern systems languages like Rust can eliminate whole classes of bugs at compile time. The course acts as a bridge to advanced topics such as operating systems, high‑performance computing, and distributed systems.

## Learning Outcomes
- Understand the structure of an object file, linking, and the compiler toolchain (`gcc`, `rustc`, `make`, `cargo`).
- Use `gdb`, `valgrind`, and sanitizers to debug and profile system programs.
- Master memory layout, pointer arithmetic, and manual memory management in C.
- Apply Rust’s ownership, borrowing, and smart pointers (`Box`, `Rc`) to the same low‑level concepts.
- Write a custom memory allocator in C, handling alignment and fragmentation.
- Understand the memory hierarchy (caches, false sharing) and write cache‑friendly code.
- Exploit SIMD auto‑vectorization with compiler flags.
- Create and synchronise processes (`fork`, `exec`) and POSIX threads.
- Build safe concurrent programs in Rust using `Arc<Mutex<T>>`, channels, and `Send`/`Sync` traits.
- Design non‑blocking I/O, event loops, and asynchronous applications with `epoll` and `tokio`.
- Use inter‑process communication: pipes, shared memory (`mmap`/`shm`), and signals.
- Implement lock‑free data structures and understand memory ordering.
- Develop a substantial system tool (shell, concurrent server, or memory allocator) and present it as a final project.

## Textbooks and Course Material
- No required textbook.
- Lecture notes will be posted weekly on Google Classroom, based on:
  - [System Programming Wiki (Angrave, UIUC)](https://github.com/angrave/SystemProgramming/wiki)
  - [CS341 Coursebook (UIUC)](https://cs341.cs.illinois.edu/coursebook/index.html)
  - *The Linux Programming Interface* by Michael Kerrisk ([man7.org](https://man7.org/tlpi/index.html))
  - *The C Programming Language*, Kernighan & Ritchie (for C reference)
- Rust resources (free online):
  - [The Rust Programming Language (Book)](https://doc.rust-lang.org/book/)
  - [Rust by Example](https://doc.rust-lang.org/rust-by-example/)
  - [The Rustonomicon](https://doc.rust-lang.org/nomicon/) (for unsafe Rust)

## Weekly Content
*(Slight changes may occur during the semester.)*

1. **Course intro, modern Linux architecture, toolchains**  
   - C/C++/Rust toolchains: `gcc`, `rustc`, `make`, `cargo`, `gdb`.  
   - Build systems, debugging workflows, and version control (Git).

2. **Low‑level execution & ABI**  
   - Calling conventions, stack frame mechanics, assembly basics.  
   - Function pointers in C and Rust.

3. **Memory layout & pointer mechanics**  
   - Virtual memory space, stack vs. heap allocation.  
   - Raw pointers vs. Rust smart pointers (`Box`, `Rc`), references and ownership.

4. **Custom memory allocation**  
   - Writing a `malloc`/`free` allocator in C.  
   - Memory alignment, fragmentation, and coalescing.

5. **Cache hierarchies & data locality**  
   - L1/L2/L3 caches, cache lines, false sharing.  
   - Structure packing and padding (`__attribute__((packed))` in C, `#[repr(C)]` in Rust).

6. **SIMD & vectorization**  
   - Auto‑vectorization flags (`-O3`, `-mavx2`).  
   - Brief introduction to SIMD intrinsics (optional).

7. **Processes & threads**  
   - `fork()`, `exec()`, POSIX threads (`pthread_create`).  
   - Rust thread spawning and basic `std::thread` usage.

8. **Midterm Exam**

9. **Synchronization & atomics**  
   - Mutexes, condition variables, semaphores.  
   - Rust’s `Arc<Mutex<T>>`, `RwLock`, and the concept of interior mutability.

10. **Concurrent data structures**  
    - Thread‑safe queues, race conditions, deadlock avoidance.  
    - Compile‑time race prevention: `Send` and `Sync` traits in Rust.

11. **Asynchronous programming & I/O**  
    - Non‑blocking I/O, `epoll`, event loops.  
    - Rust’s `async`/`await` and the `tokio` runtime.

12. **Inter‑process communication**  
    - Pipes, FIFOs, shared memory (`shmget`, `mmap`), signals.

13. **Advanced hardware optimization**  
    - Lock‑free algorithms, memory barriers, relaxed atomic models.  
    - Practical examples with C `stdatomic.h` and Rust’s `std::sync::atomic`.

14. **Final project presentations**  
    - Students present a systems tool / shell / concurrent server / memory allocator showcase.

## Quizzes and labs
- Assigned weekly via Google Classroom and GitHub Classroom.
- Typically multiple‑choice and short programming exercises related to the week’s topic.
- Labs will mix C and Rust exercises.

## Homework-coding assignments
- ~3‑4 programming assignments (including a group project).
- Submissions through Google Classroom and GitHub Classroom.
- No late submissions.

## Grading 
- 10% labs/quizzes (lowest one dropped)
- 20% midterm exam
- 30% assignments (group projects)
- 40% final exam

## Course sites
- Assignment grading and small discussions: Google Classroom.
- Other discussions and Q&A: Piazza (access code provided in class).
- Submissions: on github

## Environment Settings
- You **must** have a Linux environment (any distribution) or a Unix‑like system (macOS, WSL2).
- For Windows: WSL2 with `build-essential`, or a virtual machine (VirtualBox / UTM).
- Install the standard C toolchain and **Rust** (`rustup`, `cargo`):
  - `curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh`
- Install `valgrind` (`sudo apt install valgrind`).
- Have a GitHub account (you can use your university email for benefits).

## Collaboration and Cheating Policy
Plagiarism and cheating are strictly forbidden (see university policy).  
You may discuss assignments with classmates, but all submitted work must be your own.  
List any collaborators and references used in your submission.

## Use of AI, GPT, Gemini, DeepSeek, etc. 
You are free to use AI tools – they can be excellent learning companions. However, over‑reliance may prevent you from developing your own problem‑solving muscles. Use them to learn, not to skip thinking.

**The golden rule:** You must always be able to explain every line of code you submit, and you must have written (and understood!) the core logic yourself.

Below is a guide on how to use AI responsibly for this course.

### Example Uses and Prompts

#### C and Rust Programming Basics
- *“Write a C program that demonstrates pointer arithmetic on an array. Then rewrite the same algorithm in safe Rust using slices. Explain the differences in memory safety and performance.”*
- *“Show me how to use `valgrind` to detect a memory leak in my C code. Then run `cargo miri` on a Rust snippet that uses `unsafe` and explain what it reports.”*

#### System Calls and OS Interaction
- *“Generate a minimal C example using `fork()` and `exec()` to run a child process, and show how to avoid zombie processes with `waitpid`.”*
- *“My `read()` call keeps returning -1 with `errno == EINTR`. How do I handle this correctly in a signal‑heavy program?”*
- *“Rewrite this C `fork`/`pipe` example in Rust using `std::process::Command` and `std::io::pipe`. Which version feels safer and why?”*

#### Rust Ownership, Smart Pointers, and Lifetimes
- *“Explain why this Rust code doesn’t compile due to ownership rules, and suggest at least two ways to fix it (e.g., clone, borrow, `Rc<RefCell<T>>`).”*
- *“Convert a C doubly‑linked list to Rust using raw pointers and `unsafe`. Then try to redesign it using safe Rust with `Option<Box<Node>>`. What are the tradeoffs?”*
- *“I keep getting ‘use of moved value’ errors. Can you draw a timeline of ownership moves in my code and show me how to restructure it?”*

#### Memory Management and Custom Allocators
- *“Outline a simple first‑fit memory allocator in C using an implicit linked list. What are the tradeoffs compared to best‑fit or next‑fit?”*
- *“How does Rust’s `GlobalAlloc` trait let me plug in a custom allocator? Give a minimal working example that tracks allocation count.”*

#### Concurrency and Synchronization
- *“Show a producer‑consumer solution in C with `pthread_mutex_t` and `pthread_cond_t`. Then show the equivalent in Rust using `std::sync::mpsc` channels. Which is less error‑prone and why?”*
- *“I have a data race in my multi‑threaded C program. Help me use ThreadSanitizer (`-fsanitize=thread`) to pinpoint it. Then explain how Rust would have caught the same bug at compile time.”*
- *“What do the `Send` and `Sync` traits mean in Rust? Why does the compiler refuse to send a `MutexGuard` across threads?”*

#### Asynchronous I/O and Networking
- *“Write a simple TCP echo server using `epoll` in C. Then rewrite it in Rust using `tokio::net::TcpListener` with async/await. Compare code complexity, safety, and performance.”*
- *“My non‑blocking socket server is spinning the CPU at 100%. How do I properly use `epoll` edge‑triggered mode to avoid this?”*
- *“What is the difference between `select`, `poll`, and `epoll`? When should I use each?”*

#### SIMD and Low‑Level Optimization
- *“Explain what the `-mavx2` flag does and how I can check if my loop was auto‑vectorized by the compiler (using `objdump` or compiler optimization reports).”*
- *“Give me a short Rust snippet using `std::arch::x86_64::_mm256_add_ps` to add two arrays of floats. How do I ensure alignment?”*

#### Debugging and Tooling
- *“I’m seeing ‘Broken Pipe’ errors in my client‑server program. What usually causes this, and how do I fix it?”*
- *“Decode this `strace` output: [paste]. Where is my program blocking, and why isn’t it responding?”*
- *“My Rust program panics with ‘already borrowed: BorrowMutError’. Can you trace through my `RefCell` usage and show me how to avoid the runtime panic?”*

#### General Conceptual Learning
- *“Summarize the differences between thread‑based concurrency and async/await event loops. When should I choose one over the other?”*
- *“I’m stuck understanding false sharing. Can you draw a diagram and show how padding fixes it, both in C and in Rust?”*
- *“Compare the strengths and weaknesses of C versus Rust for system programming. What concrete safety guarantees does Rust provide that C lacks?”*

**Final reminder:** AI tools are great for generating examples, explaining error messages, and helping you explore “what if” scenarios – but the real learning happens when you struggle with a problem, fix your own bugs, and internalize *why* things work. Use AI as a smart study partner, not as a substitute for your own thinking.