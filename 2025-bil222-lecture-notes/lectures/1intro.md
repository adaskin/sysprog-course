---
title: "System Programming - Introduction"
author: "Ammar Daskin"
institute: "Istanbul Medeniyet University, Computer Engineering Department"
marp: true
paginate: true
size: 16:10
theme: default
class: invert
---
<style type="text/css">
div {
  font-size: clamp(10px, 2vw, 32px);
}
</style>

# 🎓 BIL 222 System Programming - Spring 2024  
**Instructor: Ammar Daskin**  
**📍 Classroom: B517**  


---

## 🕒 Course Hours  
- **Lectures: Thursday @ 8:30 AM**:  
  - Slides & examples posted on **canvas**.  
- **Labs: Wednesdays**:  
  - B-Computer Labs
  - Labs/Quizzes via **GitHub** or in-class tools.  

---

## ✅ Prerequisites  
- **Mandatory**: Passing grade in **BIL 121 (C Programming)**.  
- **Recommended Knowledge**:  
  - Basic data structures & algorithms  
  - Computer architecture basic fundamentals (BIl 111)  

---

## 📚 Resources & Textbooks  
| Type              | Resources                                                                 |
|-------------------|---------------------------------------------------------------------------|
| **Core Textbook** | [System Programming Wiki](https://github.com/angrave/SystemProgramming/wiki) |
| **Linux Guide**   | [The Linux Programming Interface](https://man7.org/index.html) (Kerrisk) |
| **C Programming** | *The C Programming Language* (Kernighan & Ritchie)                       |
| **Advanced**      | *Linux System Programming* (Love), *Advanced UNIX Programming* (Stevens) |

---

## 📊 Grading Breakdown  
| Category                  | Weight | Details                               |
|---------------------------|--------|---------------------------------------|
| 🧪 **Weekly Labs/Quizzes** | 10%    | Lowest grade dropped                  |
| 📝 **Programming Assignments** | 30% | 3-4 projects (individual/group)       |
| 📘 **Midterm Exam**        | 20%    | Theory & implementation               |
| 📚 **Final Exam**          | 40%    | Comprehensive assessment              |

---

## Letter grades
- $> 90$ or $\approx$ top 10% AA
- $<35$ is FF.
- median $x$, and standard deviation $2s$ (we use half)
  - BA: $\approx[x+3s, x+4s)$ 
  - BB: $\approx[x+2s, x+3s]$ 
  - BC: $\approx[x+1s, x+2s)$ 
  - CC: $\approx[x-1s, x+1s)$ 
  - DC: $\approx[x-2s, x-1s)$ 
  - Depending on the value of $s$ and $x$, some slight changes may occur...  

---

## 🚀 How to Succeed in This Course  
**Lectures**:  
- Focus on **theory** and **simple implementations**.  
- ✏️ **Take notes**—details may not be in slides!  

**Labs**:  
- Practice concepts from lectures.  
- May include **bonus topics**.  

**Projects**:  
- Implement algorithms discussed in class.  
- Submit via **GitHub** (prototypes provided).  

---

## 💻 What is System Programming?  
> "Low-level software that interfaces directly with the OS kernel: It uses core system libraries."  

**Examples**:  
- Text editors (`vim`, `emacs`)  
- Debuggers (`gdb`)  
- Network servers (`nginx`, `Apache`)  

---

## 🌟 Why Learn System Programming?  
- Write **efficient low-level code** (e.g., OS components).  
- Optimize high-level applications with kernel/low-level insights.  

---

## 🐧 Unix System Programming  
**Key Components**:  
```plaintext
User Program → System Call → Kernel → Hardware
```  
**Tools**:  
- `gcc` compiler  
- System calls for I/O, process control, and memory management. 
  - > provided by the kernel API.

---

## 📲 Protability of programs

**API:**
- Application programming interface: source code level  

**ABI:**
- Application binary interface: compiled program interface
- the same function on any system with the same ABI  

---

## 📜 Compliance & Standards  
- **POSIX**: Ensures cross-OS compatibility.  
- **ISO C99**: `gcc` compliance for portable code.  

---

## 📅 Weekly Topics  
1. 🛠️ Git, GCC, Makefiles, C Crash Course  
2. 🧠 Pointers, Memory Model, GDB, Valgrind  
3. 🔄 Processes  
4. 💾 Memory Allocators  
5. 🧵 Threads  
6. 🔒 Synchronization  
7. 📝 Midterm Review  
8. ☠️ Deadlock  
9. 🌐 Virtual Memory & IPC  
10. ⏳ Scheduling  
11. 📡 Signals 
12. 🌍 Networking/Sockets  
13. 📂 Filesystems  
14. 🎓 Final Review  

---

## This week topics
- Version Control Basics
- Git Fundamentals
- Crash course on C
  - Program files
  - Makefile Essentials

---

## What is Version Control?
A system to:
- Track changes to code/files over time
- Collaborate without overwriting work
- Revert to previous states

**Key Terms**:
- Repository: Database of project history
- Commit: Snapshot of changes
- Branch: Parallel development line
- Merge: Combine changes from branches

---

## Git Basics
**Distributed Version Control**:
- Local + remote repositories (e.g., GitHub)
- Every clone has full history

**Essential Commands**:
```bash
git init        # Create new repo
git add [file]  # Stage changes
git commit -m "message" # Save snapshot
git branch      # List/create branches
git checkout    # Switch branches
git merge       # Combine branches
git push/pull   # Sync with remote
```

---

## Typical Git Workflow
```
          commit
Work Dir ----> Staging Area ----> Repository
             (git add)         (git commit)
```

**Branching Model**:
```
main ────┐
          ├─ merge ──> deploy
dev  ────┘
```

---


## 🌿 Git Essentials (live demo)  
**Workflow**:  
```bash
# Initialize & commit
git init
git add *.c LICENSE
git commit -m "Initial commit"

# Sync with GitHub
git remote add origin https://github.com/user/repo.git
git push -u origin main
```

**Pro Tip**: Use [SSH keys](https://docs.github.com/en/authentication/connecting-to-github-with-ssh/generating-a-new-ssh-key-and-adding-it-to-the-ssh-agent) for secure authentication! 🔑  
Use [GPG keys](https://docs.github.com/en/authentication/managing-commit-signature-verification/adding-a-gpg-key-to-your-github-account) to sign your commits 

---

### Accepting git assignments

Use the given assignment link on your browser  
[https://classroom.github.com/a/?????....](https://classroom.github.com/a/?????....)  
- Choose your name in identifiers to link your github account
- One you accepted an assignment, click the assignment to open on browser

---

### Submitting your assignment

```
$ git clone https://github.com/YOUR-USERNAME/YOUR-REPOSITORY 
$ cd YOUR-REPOSITORY
# Make some changes or add files
$ echo "int main(){}" > newfile.c
$ git add newfile.c
#commit your changes
$ git commit -m "new file added!"
#push your changes to the remote
$ git push origin main
```
You have pushed the changes to remote repo on github.com and submitted your assignment

---

# Crash course on C and development tools
C-examples done in class are posted in lecture notes...

---

## Program files
A program file contains all the necessary information to load an application into memory and make it run.  
**It includes:**
- machine instructions
- initialized data
- List of library dependencies
- List of memory sections that the program will use
- List of undefined values in the executable that will be known until the
program is loaded into memory.

---

### Example with C
```C
$ cat > main.c
#include <stdio.h>
int main() {
    printf("Hello World!\n");
    return 0; 
}
$ gcc main.c & ./a.out
```
```Plaintext
 Source(.c, .h, .o, etc) → GCC → Executable(a.out)
``` 


---

## ⚙️ Compilation  Steps    
1. **Preprocess**: `gcc -E main.c -o main.i`  
2. **Compile**: `gcc -S main.i -o main.s`  
3. **Assemble**: `gcc -c main.s -o main.o`  
4. **Link**: `gcc main.o -o main`  

**Example**:  
```bash
$ ./main
Hello World! 👋
```

---


### 🔧 GCC Compilation Deep Dive: From Source to Executable  

```bash
# 🎯 Step 1: Preprocess (Expand macros/includes)
$ gcc -E main.c > main.i  # -E stops after preprocessing

# 🔄 Step 2: Generate Assembly
$ gcc -S main.c           # Creates main.s (assembly)

# ⚙️ Step 3: Assemble to Object File
$ gcc -c main.s           # Produces main.o (machine code)

# 🔍 Inspect Symbols
$ nm main.o               # Shows undefined symbols (e.g., printf)

# 🔗 Step 4: Link Object Files
$ gcc -o main main.o      # Creates executable

# ❓ Where's printf?
$ nm main                 # Dynamic linking resolves this later!
```

**Pro Tip**: Explore all GCC flags → [GCC Option Summary](https://gcc.gnu.org/onlinedocs/gcc/Option-Summary.html) 🔗

---

## Program Loading Explained  
**The Loader's Magic** ✨  

1. **Allocates Memory**:  Reserves space for `.text`, `.data`, `.bss` sections.  
1. **Loads Dependencies**:  Executable + shared libraries (if not already loaded).  
2. **Resolves Symbols**:  Patches addresses for external functions (e.g., `printf`).  
3. **Starts Execution**:  
   ```plaintext
   _start → init() → main() → exit()
   ```  
4. **Runtime Linker**: Use `ldd` or `otool -L` to check shared libs:  
   ```bash
   $ ldd a.out  # Show library dependencies
   ```

---

## 📦 Static vs. Shared Libraries  
### Battle of the Binaries 🥊  

| Feature               | Static Libraries (.a)          | Shared Libraries (.so)         |
|-----------------------|---------------------------------|---------------------------------|
| **📦 Size Impact**    | Bloats executable              | Zero executable bloat           |
| **⏳ Loading**        | Embedded at compile-time       | Loaded dynamically at runtime   |
| **💡 Memory Usage**   | Per-process copy (wasteful)    | Single system-wide instance     |
| **📎 Linking**       | Static linker (compile-time)   | Dynamic linker (runtime)        |

**Example**:  
- Static: `libmath.a` → Compiled into your binary.  
- Shared: `libc.so` → Loaded once for all programs.  

---

# 🛠️ Makefiles 101  
**Automate build processes**:
- Define tasks/rules
- Manage dependencies
- Avoid repetitive commands  

---

**Basic Syntax**:
```makefile
target: dependencies
    commands
```

**Example**:
```makefile
build: main.c utils.c
    gcc -o program main.c utils.c

clean:
    rm -f program *.o
```

---

## Why Use Makefiles?
1. **Automation**: Single command to build/test
2. **Efficiency**: Only rebuild changed components
3. **Standardization**: Consistent process for all users
4. **Flexibility**: Chain tasks (build → test → deploy)

Run with:
```bash
make build
make clean
```

---

### Automate Your Builds 🤖  

```makefile
# Simple Makefile
CC = gcc
CFLAGS = -Wall -g

build: main.c aux.c
    $(CC) $(CFLAGS) -o program main.c aux.c

clean:
    rm -f program *.o
```

---

**Key Features**:  
- 📜 Declarative rules for compiling/linking.  
- 🔄 Automatic recompilation of changed files.  
- 🧩 Supports variables and implicit rules.  

**Learn More**:  
- [Makefile Tutorial](https://makefiletutorial.com/) 🎓  
- [GNU Make Manual](https://www.gnu.org/software/make/manual/make.html) 📚  

---


## Beyond Make: Build System Alternatives  
### Choose Your Weapon ⚔️  

| Language    | Tools                          | Use Case                      |
|-------------|--------------------------------|-------------------------------|
| **C/C++**   | CMake, Bazel, Ninja, SCons     | Cross-platform, large projects|
| **Java**    | Maven, Gradle, Ant             | Dependency management         |
| **Rust**    | Cargo                          | Built-in, modern workflows    |
| **Go**      | `go build`                     | Minimalist, no config needed  |

**Did You Know?**  
- Microsoft Visual Studio uses MSBuild for C++.  
- Ninja focuses on **speed** for massive codebases. 

---

## 🧪 This Week’s Lab: Git & C  
- Create a multi-file C project.  
- Design a **Makefile** for compilation.  
- Submit via GitHub using Git CLI.  

---

## ➡️ Next Lecture Preview  
- Deep dive into **C pointers, structs...** 🎯.  
- Structs, memory alignment, and optimization.  