**Project Writeup: Multi-User Communicating Shells with Shared Messaging**  
---

### **Project Objectives**  
1. **Understand System Programming Fundamentals:** Processes, shared memory, file descriptors
4. **Utilize GUI Frameworks**: Use GTK (or similar GUI libraries) to create terminal-like windows with interactive features. 
5. **Work with Concurrency**: Handle multiple shells running in parallel and manage data written and read from the shared buffer.
6. **MVC Design**: Separate logic into Model, View, and Controller components.  



### **Project Overview**  
In this project, you will design a terminal-like application where multiple shell instances can execute commands *and* communicate with each other via a shared message buffer. Each shell instance (implemented using GTK for the GUI) will function as a standard shell (e.g., parsing commands like `ls`, `grep`, etc.) while also enabling users to send/receive messages through a shared pipe. The goal is to combine system programming concepts (processes, IPC, synchronization) with GUI development (GTK) and modular design (MVC architecture).

**Key points:**
1. **Multi-Shell Environment**: Launch `n` interactive GUI-based shell instances via a GTK-based interface (or similar).  Each shell runs as a separate process.
2. **Command Execution**: These shells allow users to execute shell commands (much like bash or sh). Each shell must execute standard commands (e.g., `ls`, `cat`, `grep`, redirection and pipes).  
   1. Alongside their primary shell functionality, these shells:
      - Can send text messages to a shared pipe/buffer that acts as a centralized communication buffer **(partial buffer implementation is given)**.

---

### **Design (MVC Architecture)**  
**Overview Design Flow**  
1. User types a command in the **View** (e.g. GTK entry widget).  
2. **Controller** parses input and decides:  
   - If it’s a shell command → call `model_execute_command()`.  
   - If it’s a message → call `model_send_message()`.  
3. **Model** handles the actual `fork/exec` or writes to the shared buffer.  
4. **Model** updates its internal state (e.g., process table, message buffer).  
5. **View** reads the model’s state (e.g., via observers) to display output.

#### **1. Model**  
Responsible for data and backend logic.  
- **Shell Process Management**  
  - Fork/exec processes for each shell instance.  
    - Executes commands using fork(), execvp(), and redirection.
  - Track child process IDs, input/output pipes, and status (active/terminated). 
    - e.g., `waitpid()` for zombies
  - Example data structure:
    ```c
    typedef struct {
        pid_t pid;          /** Process ID of command   */
        char command[256];  /** Command string          */
        int status;         /** Running/exited          */
    } ProcessInfo; 
    ```
    You should implement a function `execute_command()` that takes a command and executes it.
        - You can call this function from controller to execute a command.

- **User Session Data**  
  - Track active shells, command history (log of commands), and message history.  

- **Shared Message Buffer**  
  - Implement a shared memory for inter-shell communication.  
    - **The buffer implementation (given below)**
    ```c
    /*contents of buf_init() function in model.c*/
    int fd = shm_open(SHARED_FILE_NAME,  O_CREAT | O_RDWR, 0600);
    /*Map shared memory*/
    char *msgbuf = mmap(NULL, BUF_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    ```
    Here `shm_open` creates a shared file object (the usage is exactly the same as open), mmap maps this object to memory.**We will cover the details of this in the coming weeks.**
    **Usage**
    You can think of it similar to malloc with more options: think of the pointer `msgbuf` as an array with size `BUF_SIZE`.  
    ```c
    msgbuf[0] = 'c';
    strncpy(&msgbuf[i], ..., ...), 
    memcpy(&msgbuf[j], ..., ...) 
    ```
    Instead of `msgbuf`, in the given code, we use the following data structure:
    ```c
    /* YOU CAN CHANGE THIS */
    struct shmbuf {
        sem_t sem;  /** controls read/write */
        size_t cnt; /** number of bytes used in 'msgbuf' */
        int fd; /** file descriptor*/
        char msgbuf[]; /* data being transferred */
    };
    ```
    Then we use `struct shmbuf *shmp = mmap(...);`. This way shm->msgbuf gives the initial address of the msg while `sem`, `cnt`, and `fd` info are kept at the beginning of the memory.  
    

    **Synchronizing access to shared memory**:
    Since it is a shared resource, if all processes try to access this region it would cause data corruption.
    Use synchronization primitives `sem_wait` and `sem_post` whenever you read/write to/from the shared memory. For example: 
    ```c
    sem_wait(&shmp->sem); 
    strncpy(shmp->msgbuf, msg, sizeof(msg)); // writing or reading
    sem_post(&shmp->sem);
    ```

#### **2. View (GTK Interface)**  
Handles the GUI and user interactions.
- **Terminal Emulator Widget**  
  - Use GTK widgets (e.g., `GtkTextView`, `GtkEntry`) to mimic a terminal.  
  - Provide text input/output areas for executing commands.
  - Handle user input events (e.g., button presses, keypresses) from the GUI. 
  - Display command output and messages from the shared buffer. 
- **Shared Message Panel**  
  - A dedicated area (e.g., a sidebar) to display all messages from the shared buffer.  
    - **optional**: Messages are color-coded or prefixed with usernames.
- **Shell Tabs/Windows**  
  - Use GTK to create a terminal-like GUI window for each shell.
  -  **(optional)** Allow users to open/close shells in tabs or separate windows.  

#### **3. Controller**  
User types a command in the View (GTK entry widget). Controller parses input and decides:
- If it’s a shell command → call `model_execute_command()`.
- If it’s a message → call `model_send_message()`.
- It also read messages from the shared memory at regular intervals and update the Message Area of each shell. 
 
- **Input Handling**  
  - Parse user input: distinguish between shell commands (e.g., `ls`) and messages (e.g., `@msg Hello!`).  

- **Message Broadcasting**  
  - Write messages to the shared buffer when a user sends one.  
    - Call `model_send_message()` function to send message to the shared buffer. 
- **Output Redirection**  
  - Read stdout/stderr from shell processes and update the GUI.  
    - Call `model_read_messages()` and periodically for new messages and refreshes the View. 

---

### **Implementation Plan**  
#### **1. Shell Process Manager (model)**  
- Implement a simple shell that accepts commands, forks a child process, and executes the commands using  `fork()`, `execvp()`, 
- Implement redirections to/from stdin/stdout/stderr.  
  - You can use `dup2()` to change the stdin, stdout, stderror.
```c
/*In CHILD PROCESS*/
int new_out = open("file", O_WRONLY | O_CREAT | O_TRUNC, 0666);
/*backup stdout*/
int saved_out = dup(STDOUT_FILENO);
/*check for errors*/

close(STDOUT_FILENO);/*file descriptor 1 empty*/
dup2(new_out, 1); /*duplicate new_out to 1*/
/*check for errors*/


/*exec...*/

/*restore stdout*/
dup2(saved_out, 1); /*duplicate saved_out to 1*/

close(saved_out);
close(new_out);
``` 
Or you can use pipes (**we will see in IPC**).
```c
/* Example for capturing command output*/
... execute_command(..., char *command){
    int pipefd[2];
    pipe(pipefd);
    if (fork() == 0) {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO); /* Redirect stdout to pipe */
        execvp(command[0], command);
    } else {
        close(pipefd[1]);
        read(pipefd[0], buffer, sizeof(buffer)); /* Read output */
    }  
}
```
#### **2. Shared Message Buffer**  
- **(given)** Implement using POSIX shared memory (`shm_open`) or a named pipe (`mkfifo`).  
-  Write methods to send and receive messages from the buffer.
   -  e.g. `module_send_message(), module_read_messages()`.
-  Integrate commands to the controller

#### **3. GUI Shell (GTK)**  
- Add input fields and text views for command/message display. 
  - Use e.g. `GtkTextView` for terminal output and `GtkEntry` for command input.  
   
- **(optional)** Add syntax highlighting for messages (e.g., color-coding user messages).  

#### **4. Message Parser**  
- Detect special commands (e.g., `@msg` to send a message). 
  - Differentiate between shell commands (e.g., `cat file.txt`) and messages (e.g., `@msg Hi!`).   
- Extract and sanitize user input before writing to the shared buffer.  

#### **5. Connect Command Execution with GUI**
- Integrate the shell logic with the GUI so that users can execute commands directly from the GUI window.

#### **6. Add Messaging Functionality**
- Incorporate the shared buffer into the GUI.
- Periodically poll the shared buffer for new messages. 
  
---
 

### **Extensions (Optional)**  
1. **User Identities**: Assign unique usernames/colors to message senders.  
2. **Private Messaging**: Allow direct messages between specific shells.  
3. **History**: Implement command/message history with scrollback.  
4. **File Transfer**: Extend messaging to support file sharing.  

---

### **Submission** 
1. Source code with MVC separation.
   1. at least: Makefile, view.c, controller.c, model.c  
2. A README explaining setup/usage.  
3. A report discussing design choices and challenges.  

---

### **Grading**  
Each submission will be presented and graded in Labs. 

---

Below is the given model.c

```c
#include <fcntl.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

#define BUF_SIZE 4096
#define SHARED_FILE_PATH "mymsgbuf"

#define errExit(msg)        \
    do {                    \
        perror(msg);        \
        exit(EXIT_FAILURE); \
    } while (0)

/* YOU CAN CHANGE THIS */
typedef struct shmbuf {
    sem_t sem;     /** controls read/write */
    size_t cnt;    /** number of bytes used in 'msgbuf' */
    int fd;        /** file descriptor*/
    char msgbuf[]; /* data being transferred */
} ShmBuf;

struct shmbuf *buf_init() {
    struct shmbuf *shmp;

    /*contents of model_init() function*/
    int fd = shm_open(SHARED_FILE_PATH, O_CREAT | O_RDWR, 0600);
    if (fd < 0) {
        errExit("could not open shared file");
    }

    /* Map the object into the caller's address space. */
    shmp =
        mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    if (shmp != NULL) {
        ftruncate(fd, BUF_SIZE);
        shmp->fd = fd;
        sem_init(&shmp->sem, 1, 1);
    } else {
        errExit("mmap error");
    }

    return shmp;
}

int main() {
    struct shmbuf *shmp = buf_init();
    pid_t pid = fork();

    if (pid == 0) {
        /* child */
        char msg[100] = "hi from child";

        sem_wait(&shmp->sem); /* for synchronization */
        strncpy(shmp->msgbuf, msg, sizeof(msg));
        sem_post(&shmp->sem);

        errExit("msg sent:");
    } else if (pid > 0) {
        /* parent */
        wait(NULL);

        sem_wait(&shmp->sem); /* for synchronization */
        printf("parent received: %d-%.100s\n", shmp->fd,
               shmp->msgbuf);
        sem_post(&shmp->sem);
    }
    shm_unlink(SHARED_FILE_PATH);
    return 0;
}
```
