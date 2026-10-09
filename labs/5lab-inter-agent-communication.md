# Lab 5: Inter-Agent Communication — From File Mailboxes to True Pipes

**Course:** BIL 322 System Programming  
**Topic:** Pipes, Redirection, and Pipeline Implementation  
**Context:** Project 2 (Communicating Agents)

---

## 🎯 Learning Objectives
By the end of this lab, you will be able to:
1. Wire worker agents together using standard file I/O (the "File Mailbox" pattern).
2. Recognize the performance and concurrency limitations of file-based IPC.
3. Implement true in-memory Inter-Process Communication (IPC) using unnamed pipes (`pipe()`).
4. Surgically redirect standard input/output (`stdin`/`stdout`) via `dup2()`.
5. Build a multi-stage agent pipeline (e.g., `producer | filter | consumer`).
6. **Avoid the "EOF Hang"** — the most common deadlock in systems programming.

## 📋 Prerequisites
- Week 4 concepts: `fork()`, `exec()`, `waitpid()`, file descriptors, and `dup2()` for file redirection.
- A working C development environment with `gcc` and `make`.

---

## 🤖 Context: Project 2 and the Orchestrator
In Project 2, your orchestrator manages multiple worker agents. Initially, agents might communicate by leaving "notes" in files (e.g., `mailbox/agent.inbox`). While this works for asynchronous tasks, it is slow (disk I/O) and requires strict synchronization.

This lab upgrades your orchestrator's capabilities: we will replace file-based mailboxes with **in-memory pipes**, allowing agents to stream data to each other in real-time, exactly like a Unix shell pipeline (`cmd1 | cmd2 | cmd3`).

---

## 🛠️ Task 0: The "File Mailbox" (Simulating a Pipe)
**Duration:** 20 minutes  
**Objective:** Redirect the output of a "Producer" agent to a file, and have a "Consumer" agent read from it.

### Background
Before true pipes, we simulate streaming by using a temporary file. 
1. **Producer Agent** writes its output to `mailbox/temp.txt`.
2. **Consumer Agent** reads its input from `mailbox/temp.txt`.

*Note: This is **not** equivalent to a true pipe. It requires disk I/O, and the Consumer cannot start processing until the Producer is completely finished.*

### Steps
1. **Setup**: Ensure a `mailbox/` directory exists.
2. **Fork the Producer**: 
   - Create/Open `mailbox/temp.txt` with `O_WRONLY | O_CREAT | O_TRUNC`.
   - Redirect `stdout` to this file using `dup2()`.
   - Execute a data-generating command (e.g., `find . -type f -name "*.c"`).
3. **Wait**: The Orchestrator (parent) *must* `waitpid()` for the Producer to finish.
4. **Fork the Consumer**:
   - Open `mailbox/temp.txt` with `O_RDONLY`.
   - Redirect `stdin` to this file using `dup2()`.
   - Execute a processing command (e.g., `wc -l` to count the files).
5. **Cleanup**: `unlink("mailbox/temp.txt")` to delete the temporary file.

### 🧠 Discussion Question
*If the Orchestrator forgot to `wait()` for the Producer and launched the Consumer immediately, what would the Consumer count? Why?*

---

## 🚰 Task 1: The True Pipe (In-Memory Streaming)
**Duration:** 30 minutes  
**Objective:** Create an in-memory pipe and wire two worker agents together concurrently.

### Steps
1. **Create a pipe**: `int fd[2]; pipe(fd);`
2. **Fork the Producer (Child 1)**:
   - Close the read end (`fd[0]`).
   - `dup2(fd[1], STDOUT_FILENO)`.
   - Close the original write end (`fd[1]`).
   - `exec` the Producer command (e.g., `find . -type f -name "*.c"`).
3. **Fork the Consumer (Child 2)**:
   - Close the write end (`fd[1]`).
   - `dup2(fd[0], STDIN_FILENO)`.
   - Close the original read end (`fd[0]`).
   - `exec` the Consumer command (e.g., `wc -l`).
4. **The Orchestrator (Parent)**:
   - **CRITICAL**: Close *both* `fd[0]` and `fd[1]`.
   - `waitpid()` for both children.

### 💻 Skeleton Code
```c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

int main() {
    int fd[2];
    if (pipe(fd) == -1) { perror("pipe"); exit(1); }

    pid_t p1 = fork();
    if (p1 == 0) {
        // TODO: Child 1 (Producer) setup and exec
        // Hint: close(fd[0]); dup2(fd[1], 1); close(fd[1]);
        // execlp("find", "find", ".", "-type", "f", "-name", "*.c", NULL);
    }

    pid_t p2 = fork();
    if (p2 == 0) {
        // TODO: Child 2 (Consumer) setup and exec
        // Hint: close(fd[1]); dup2(fd[0], 0); close(fd[0]);
        // execlp("wc", "wc", "-l", NULL);
    }

    // TODO: Orchestrator (Parent) cleanup
    // 1. Close both ends of the pipe!
    // 2. waitpid for p1 and p2
    
    return 0;
}
```

### ⚠️ The "EOF Hang" (Lecture Tie-In)
**What happens if the Orchestrator forgets to close `fd[1]`?**
The Consumer (`wc -l`) will read data, but when the Producer finishes, `wc` will **block forever** waiting for more data. 
*Why?* Because the Orchestrator still holds the write end open. The kernel assumes the Orchestrator *might* write more data later, so it never sends an `EOF` (End of File) signal to the Consumer. 
**Rule:** *EOF happens only when EVERY write end, in EVERY process, is closed.*

---

## 🏗️ Task 2: The Multi-Stage Agent Pipeline (Challenge)
**Duration:** 40 minutes  
**Objective:** Chain three commands using two pipes. 
**Pipeline:** `find . -type f -name "*.c" | grep "main" | wc -l`
*(Find all C files -> Filter for those containing "main" -> Count them)*

### Architecture
You need **two pipes** and **three child processes**.
- **Pipe A** connects Producer -> Filter.
- **Pipe B** connects Filter -> Consumer.

### Steps
1. Create `pipe_A` and `pipe_B`.
2. **Fork Child 1 (Producer)**: 
   - Writes to `pipe_A`. (Closes all other fds).
3. **Fork Child 2 (Filter)**: 
   - Reads from `pipe_A`, Writes to `pipe_B`. (Closes all other fds).
4. **Fork Child 3 (Consumer)**: 
   - Reads from `pipe_B`. (Closes all other fds).
5. **Orchestrator (Parent)**: 
   - Closes ALL 4 file descriptors (`pipe_A[0]`, `pipe_A[1]`, `pipe_B[0]`, `pipe_B[1]`).
   - Reaps all three children.

### 🐛 Common Pitfalls
1. **Fd Leaks**: If Child 2 forgets to close `pipe_A[1]`, Child 3 will hang waiting for Child 2 to finish writing, even if Child 1 is already dead.
2. **Order of `dup2` and `close`**: Always `dup2` *before* you close the original fd, or you'll overwrite the wrong descriptor.
3. **Exec Failure**: Always `perror` and `_exit(127)` if `exec` fails in a child. If you just `return`, you will accidentally run the Orchestrator's code inside the child!

---

## 📊 Assessment & Grading

| Task | Requirement | Points |
| :--- | :--- | :--- |
| **Task 0** | File mailbox works, temp file is cleaned up. | 20% |
| **Task 1** | True pipe works, Orchestrator closes its fds (no hangs). | 30% |
| **Task 2** | 3-stage pipeline executes correctly and exits cleanly. | 40% |
| **Code Quality** | Proper error checking (`pipe`, `fork`, `dup2`), no fd leaks. | 10% |

*Grader's first test:* `ps aux | grep defunct` (Checking for zombies) and running the program multiple times to ensure no deadlocks occur.

---

## 🚀 Further Exploration (Bonus)
1. **Arbitrary Pipelines**: Modify your code to accept an array of commands and dynamically create $N-1$ pipes and $N$ children.
2. **Bidirectional Pipes**: Can you create a setup where the Orchestrator sends a query to an Agent, and the Agent sends a response back? (Hint: You need two separate pipes, one for each direction).

---

## 📝 Lab Report Submission
Submit your C source files via GitHub. Include a brief `README.md` answering:
1. Explain *exactly* why the Orchestrator must close its copies of the pipe file descriptors in Task 1. What system call inside the Consumer relies on this?
2. Draw the File Descriptor table for Task 2 (Child 2 specifically) right before it calls `exec()`. Which fds are open, and what do they point to?