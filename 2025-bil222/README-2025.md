BIL 222 System Programming(Spring 2025, Dept of Computer Engineering, Istanbul Medeniyet Unv.)
------------------------------------------------------------------------------
* [2025 lecture notes](lectures/)
* [2024 google-slide notes](https://sites.google.com/view/adaskin/system-programming) 

---

- [BIL 222 System Programming(Spring 2025, Dept of Computer Engineering, Istanbul Medeniyet Unv.)](#bil-222-system-programmingspring-2025-dept-of-computer-engineering-istanbul-medeniyet-unv)
- [Meeting Times](#meeting-times)
- [Prerequisites](#prerequisites)
- [Overview](#overview)
- [Learning Outcomes](#learning-outcomes)
- [Textbooks and Course Material](#textbooks-and-course-material)
- [Weekly Content (you might see slight changes during the semester.)](#weekly-content-you-might-see-slight-changes-during-the-semester)
- [Quizzes and labs](#quizzes-and-labs)
- [Homework-coding assignments](#homework-coding-assignments)
- [Grading](#grading)
- [Course sites](#course-sites)
- [Environment Settings](#environment-settings)
  - [Install Valgrind package](#install-valgrind-package)
  - [Open a github account (If you do not have one)](#open-a-github-account-if-you-do-not-have-one)
- [For discussions,](#for-discussions)
- [Collaboration and Cheating Policy](#collaboration-and-cheating-policy)
- [Use of AI, GPT, Gemini, DeepSeek, etc.](#use-of-ai-gpt-gemini-deepseek-etc)
  - [Example use and prompts:](#example-use-and-prompts)

## Meeting Times
- Labs on wednesdays,
- Lecture on Thursdays @517
## Prerequisites
You are expected to have a prior C programming experience and basic understanding of Data Structure and Algorithms. A passing grade in BIL 121 is required to take this course. 
## Overview
This course introduces students to the basics of system programming in Unix. In particular in the first part, it covers advanced C programming topics such as macros, function pointers, structs, memory alignment, debugging and versioning (Git, GDB, Valgrind etc.), internals of an object file, linking.  In the remaining part it teaches how to use system calls in Unix operating systems to write programs for file manipulation, process controls, threads and synchronization; and explains how to deal with the issues that can be risen in these programs.   
This course goes through systems from a programmer’s perspective: That is at the boundary of the user and kernel spaces, it shows how to interact with the systems from the user space.  Therefore, it prepares students for more advanced courses such as operating systems and computer architectures that discuss the internals of a computer system. 
During the semester, in addition to weekly lectures and lab sessions, there are programming assignments and in class quizzes. It is crucial for students to follow weekly lectures and labs to learn the material and complete the assignments. 
## Learning Outcomes
- Knowledge of the topics listed in the weekly content below.
- Ability to understand the differences of a program file, an object file, and a source file
- Ability to use git and similar distributed version control mechanisms
- Ability to use GDB, Valgrind and similar debug tools for system programs
- Ability to understand the dynamic/static linking in program files
- Ability to understand how Unix I/O works
- Ability to write a Unix shell program and direct its output to different sources
- Ability to write multi threaded programs and synchronize their accesses to a shared memory
- Ability to synchronize process mechanism by using inter-process communication mechanism such as pipes, shared memories, and signals
- Ability to write programs that communicate through networks by using sockets and different protocols.
- Ability to write a memory allocator by using implicit/explicit linked list and pointer arithmetic.
## Textbooks and Course Material
- No required textbook
- Lectures notes weekly posted on classroom.google.com and generally based on the course-books  
    - https://github.com/angrave/SystemProgramming/wiki, by Lawrence Angrave from CS @ Illinois and represents CS241 "Intro to System Programming" course content 
    - or https://cs341.cs.illinois.edu/coursebook/index.html 
    - The Linux Programming Interface: A Linux and UNIX System Programming Handbook by Michael Kerrisk.
      - https://man7.org/tlpi/index.html 
      - https://man7.org/index.html  by Michael Kerrisk.
- For C programming, you can refer to "The C Programming Language", by Brian W. Kernighan, Dennis M. Ritchie.
  
## Weekly Content (you might see slight changes during the semester.)

* [2025 lecture notes](lectures/)
* [2024 google-slide notes](https://sites.google.com/view/adaskin/system-programming) 
1. Course administrivia,  crash course on C, compiler steps, object files
2. Macros, Makefiles, Gdb, Version Control (Git)
3. Review of  pointers, pointer arithmetic, function pointers
4. C memory model, struct alignment 
5. Fundamentals of system programming & File I/O 
6. Processes
7. Memory allocators
8. midterm
9. Intro to Threads and Parallel Computation (dask and data parallelism, coroutines, green threads, goroutines) 
10. Synchronization (mutexes, condition variables, monitors, thread safe data structures)
11. Reader/writer problem, Deadlock, livelock
12. A brief discussion of virtual memory & Inter process communication (pipes/mmap/mqueues)
13. Networking(Network Protocols, TCP, UDP, HTTP, WebSocket etc.)-Socket programming-I  
14. Signals & Socket programming-II, Filesystems
15. Final-Review

## Quizzes and labs
- Assigned through https://classroom.github.com/ or google classroom 
- Expected to be every week 
- Generally multiple choice questions and short programs related to  the weekly-content

## Homework-coding assignments
- Assigned via https://classroom.google.com/   
- ~3 or 4 Programming Assignments (including group projects)
- Submissions through https://classroom.google.com/    & https://classroom.github.com/ 
- No late submission
## Grading 
- 10% labs/quizzes (given every week, 1 lowest  is not included)
- 20% midterm
- 30% 3-4 programming assignments including a group project
- 40%  final exam

## Course sites
- For the assignment submission/grading and small discussions, we use classroom.google.com, for other discussions we use piazza.com for this course.  In discussions and questions:
- Do not post solutions or any significant part of an assignment.
- Do not post anything not related to the course.
- Ask a question when you would like some help with something
- Post something when you would like to help others with something.

## Environment Settings
- You should have Linux (any distro) or Unix (BSD, macOS) installed on a machine/virtual-machine. 

In Windows,
- you can try many linux-distros through Windows subsytem for Linux https://learn.microsoft.com/en-us/windows/wsl/
  - remember to install build-essential package
- For GUI, https://learn.microsoft.com/en-us/windows/wsl/tutorials/gui-apps 

Or you can install Linux through a virtual-machine software 
- e.g. VirtualBox https://www.virtualbox.org/  
- https://ubuntu.com/tutorials/how-to-run-ubuntu-desktop-on-a-virtual-machine-using-virtualbox#1-overview
- https://wiki.debian.org/VirtualBox
- some online sites providing a linux terminal (not tested!)
- For mac M chips use https://mac.getutm.app/ 

### Install Valgrind package 
  - eg, in debian $sudo apt install valgrind
  -  https://valgrind.org/docs/manual/quick-start.html 

### Open a github account (If you do not have one)
- you can use your ####@imu.edu.tr, and get a free account https://docs.github.com/en/get-started/start-your-journey/about-github-and-git
- git reference: https://git-scm.com/docs 

## For discussions,
- https://piazza.com/istanbul_medeniyet_university/spring2025/bil222 
  - The code is given in the classroom.

## Collaboration and Cheating Policy
Any kind of plagiarism and cheating are prohibited (Please, refer to the university cheating policy).
If you benefit from some work of others, list them as references (online references or books) 
Discussing the assignments or projects with your friends is allowed; but, all the submitted work should be yours alone. List your collaborators (if you discuss your homework with your friends) in your assignments.

## Use of AI, GPT, Gemini, DeepSeek, etc. 
You are free to use them... However,over-reliance may prevent you from thinking.
 
Below (**written with the help of DeepSeek R1**) is example guide how you can leverage such tools for this course 
* You can use AI's capabilities for explanations, code examples, debugging, and interactive learning.

### Example use and prompts:
 - **Unix Basics** generate shell script examples (e.g., `ls -l` analysis)
 - **System Calls & C Programming**  `open`, `read`, `write`, `close`, `fork`, `exec`, `wait`, `signal`, etc. 
   - Ask for code snippets (e.g., "Write a C program using `fork()`"). 
   - Explain error handling (e.g., "Why does `fork()` return -1?"). 
- **Process Management** Process creation, termination, zombie processes. 
   - Simulate process trees. 
   - Debug infinite loops or orphaned processes. 
- **Inter-Process Communication (IPC)** Pipes, FIFOs, shared memory, message queues. 
   - Generate code for producer-consumer problems. 
   - Explain race conditions and synchronization. 
 - **Signals** Signal handlers, masking, `sigaction`. 
   - Demonstrate safe signal handling practices. 
   - Debug issues like signal interruption of system calls. 
- **Networking & Sockets** TCP/UDP sockets, `select`, `poll`. 
   - Create a simple client-server chat example. 
   - Troubleshoot "Address already in use" errors. 
  
  #### **General prompts related to Lectures & Theory** 
  - Use AI to find resources
  - Is this use of `select()` efficient?
  - What's the difference between `fork()` and `vfork()`?
  #### **Prompts for Code Examples** 
  - Write a C program that uses `pipe()` to send data between parent and child processes.
  ####  **Labs & Assignments** 
  You should write your code yourself. But, you can take help from AI.
  - Check the correct use: e.g. Is this use of `select()` efficient?
  - You can use AI to generate test cases (e.g., for a task of copying files, test cases that use empty files, large files).
  - You can analyze code dumps, errors, etc. Pair AI with `gdb`, `valgrind`, and `strace`
    - e.g. prompt: Why does this code create a zombie process? 
    - Explain `strace`/`gdb` outputs. P
    - e.g. I’m getting ‘Broken Pipe’ errors in my client-server program. How do I fix it?
