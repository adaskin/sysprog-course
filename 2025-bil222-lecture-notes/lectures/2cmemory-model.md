---
title: "System Programming - C memory model"
author: "Ammar Daskin"
marp: true
paginate: true
size: 16:10
theme: default
class: invert
---

<style type="text/css">
div {
  font-size: clamp(10px, 3vw, 32px);
}
</style>

# C programming, memory model and more
    - Preprocessor Directives & Macro Pitfalls
    - Pointer Arithmetic & Function Pointers
    - Memory Alignment & Struct Packing
    - Bitfields
    - Binary File I/O
    - System Calls vs Library Functions
    - Common Bugs & Debugging Techniques

  - **Resources**:
    - [https://github.com/angrave/SystemProgramming/wiki](https://github.com/angrave/SystemProgramming/wiki)
    - [GCC Documentation](https://gcc.gnu.org/onlinedocs/)
    - [Structure Packing Guide](http://www.catb.org/esr/structure-packing/)
    - Edited with DeepSeek-r1

---

#### Compiler Directives
- `gcc -E main.c > main.i`: Stops after preprocessing  
- `gcc -S main.c`: Generates assembly file  
- `gcc -c main.c`: Compiles to object file (`main.o`)  
- `nm main.o`: Lists symbols in object file  

Example:  

```c
#include <stdio.h>
int main() {
    printf("Hello World!\n");
    return 0;
}
```

---

#### Macros: Object-like
- Simple text substitution.  

```c
#define MAX_LENGTH 10
char buffer[MAX_LENGTH]; 
// Expands to: char buffer[10];
```

Multi-line macro:  

```c
#define NUMBERS 1, \
                2, \
                3
int x[] = { NUMBERS }; 
// Expands to: int x[] = { 1, 2, 3 };
```

---

#### Macros: Function-like
- Resemble function calls but are expanded inline.  
```c
#define min(X, Y) ((X) < (Y) ? (X) : (Y))
x = min(a, b); 
// Expands to: x = ((a) < (b) ? (a) : (b));
```

**Pitfall**: Operator precedence issues.  
```c
#define ceil_div(x, y) (x + y - 1) / y
a = ceil_div(b & c, sizeof(int)); 
// Expands to: (b & c + sizeof(int) - 1) / sizeof(int); 
// Fix: Add parentheses: #define ceil_div(x, y) ((x) + (y) - 1) / (y)
```

---

#### Macro Pitfalls
1. **Side Effects**:  
```c
#define min(a, b) a < b ? a : b
min(x++, 5); // Expands to x++ < 5 ? x++ : 5 (increments x twice!)
```

2. **Duplication**:  
```c
#define min(X, Y) ((X) < (Y) ? (X) : (Y))
min(x + y, foo(z)); // foo(z) may execute twice!
```

3. **Semicolon Issues**:  
```c
#define SKIP_SPACES(p, limit) { ... }
if (*p != 0)
    SKIP_SPACES(p, lim); // Adds extra semicolon, breaking else
else ...
// Fix: Use `do { ... } while(0)`
```

---

#### Macro Conditionals
```c
#ifdef __GNUC__
    return 1; // Compiled with GCC
#else
    return 0;
#endif
```

---

#### C Keywords & Operators
- **Keywords**: `extern`, `const`, `static`, `sizeof`, `struct`, etc.  
- **Operators**: Precedence hierarchy:  
  1. `[]` (array subscript)  
  2. `->`, `.` (member access)  
  3. `++`, `--` (postfix)  
  4. `+`, `-` (unary)  
  5. `*`, `/`, `%`  
  6. `<<`, `>>`  
  7. `==`, `!=`  
  8. `&&`, `||`  

---

#### Data Types
- Basic types: `char`, `short`, `int`, `long`, `float`, `double`  
- Fixed-width integers: `int32_t`, `uint64_t` (from `<stdint.h>`)  
- **Shift Operators**:  
  - Signed right shift: Sign-extended  
  - Unsigned right shift: Zero-filled  

```c
unsigned short uns = -127; // 0xFF81
uns >> 2; // 0xFFE0 (sign-extended)
short sig = 1;
sig >> 2; // 0x0000 (zero-filled)
```

---

#### Pointers
- Declaration pitfalls:  
```c
int* ptr1, ptr2; // ptr2 is an int, not a pointer!
int *ptr3, *ptr4; // Both pointers
```

- Arithmetic:  
```c
char *ptr = "Hello";
ptr += 2; // Points to 'l'
int *bna = (int*)ptr;
bna += 1; // Advances by sizeof(int) bytes (e.g., 4)
```

- **Void Pointers**:  
  - No arithmetic allowed (POSIX restriction).  
```c
void *p = malloc(10);
char *s = p; // Valid cast
```

---


### C Strings and Memory Layout

---

#### C Strings  
- **String Constants**: Stored in read-only data section.  
  ```c
  char *str = "constant"; // Read-only, modifying causes SEGFAULT
  ```
- **Mutable Strings**: Stored on stack (local) or data segment (global).  
  ```c
  char str[] = "mutable"; // Modifiable (stack/data segment)
  char *heap_str = malloc(10); // Modifiable (heap)
  ```

---

#### Program Memory Layout  
1. **Stack**:  
   - Stores automatic variables, function frames.  
   - Grows downward.  
2. **Heap**:  
   - Dynamic memory (e.g., `malloc`, `free`).  
   - Managed via `sbrk`/`brk` system calls.  
3. **Text Segment**:  
   - Read-only executable code.  
4. **Data Segment**:  
   - Initialized global/static variables.  
5. **BSS Segment**:  
   - Zero-initialized global/static variables.  

---

#### String Constants vs. Arrays  
```c
char *s0 = "abc"; // Same address for identical literals
char *s1 = "abc"; 
char s2[] = "abc"; // Separate mutable copies
char s3[] = "abc";
char s4[] = {'a','b','c','\0'};

// Output (example addresses):
// 0x563610d0f004, abc (s0 and s1 share address)
// 0x7ffc82bc7d9c, abc (s2, s3, s4 have unique addresses)
```

---

#### Constant Pointers vs. Pointers to Constants  
- **Constant Pointer**:  
  ```c
  int *const ptr = &a; // Pointer value cannot change
  // ptr = &b; // Compile error
  ```
- **Pointer to Constant**:  
  ```c
  const int *ptr = &a; // Data cannot change via pointer
  // *ptr = 10; // Compile error
  ```
- **Constant Pointer to Constant**:  
  ```c
  const int *const ptr = &a; // Neither can change
  ```

---

#### Function Pointers  
- Syntax:  
  ```c
  int (*compare)(void*, void*); // Pointer to function returning int
  ```
- Example:  
  ```c
  int intcmp(int *a, int *b) { return *a > *b; }
  int main() {
    int (*comp)(void*, void*) = (int (*)(void*, void*)) &intcmp;
    int z = comp(&x, &y); // Polymorphic call
  }
  ```

---

#### Structs and Pointers  
```c
typedef struct { int a1; int a2; } pair;
pair obj = {10, 15};
pair *ptr = &obj;
*ptr = (pair){0, 0}; // Overwrite struct via pointer
printf("a1: %d, a2: %d\n", ptr->a1, ptr->a2); // 0, 0
```

---

#### Invalid Pointer Arithmetic  
```c
pair obj = {10, 15};
pair *ptr2 = ptr + 1; // Undefined behavior (no next struct)
```

---

#### Memory Model Example  
```c
pair obj = {10, 15};
int *ptr = (int*)&obj;
printf("a1: %d, a2: %d\n", *ptr, *(ptr+1)); // 10, 15
```
Memory layout:  
```
+---------+---------+
| 10 (a1) | 15 (a2) |
+---------+---------+
```

---

### C Memory Model: Structure Alignment and Padding
See [The Lost Art of Structure Packing](http://www.catb.org/esr/structure-packing/)

---

#### Alignment Requirements  
- **Self-aligned** types (modern processors):  
  - `char`: Any address  
  - `short`: Even address  
  - `int`/`float`: Address divisible by 4  
  - `long`/`double`: Address divisible by 8  
  - Pointers: 4/8 bytes (32/64-bit systems)  

Example:  
```c
struct foo1 {
    char *p;  // 8 bytes
    char c;   // 1 byte
    long x;   // 8 bytes (forces 7-byte padding)
};
// Total size: 24 bytes (8 + 1 + 7 padding + 8)
```

---

#### Padding to Meet Alignment  
```c
struct unoptimized {
    char c;    // 1 byte
    int x;     // 4 bytes (needs 3-byte padding after c)
};
// Size: 8 bytes (1 + 3 padding + 4)
```

Optimized by reordering:  
```c
struct optimized {
    int x;     // 4 bytes
    char c;    // 1 byte (no trailing padding needed)
};
// Size: 5 bytes (but may round up to 8 for alignment)
```

---

#### Structure Reordering  
- Minimize padding by ordering members **from largest to smallest**.  

**Inefficient**:  
```c
struct foo10 {
    char c;          // 1 byte (+7 padding)
    struct foo10 *p; // 8 bytes
    short x;         // 2 bytes (+6 padding)
};
// Size: 24 bytes
```

**Optimized**:  
```c
struct foo11 {
    struct foo11 *p; // 8 bytes
    short x;         // 2 bytes
    char c;          // 1 byte (+5 padding)
};
// Size: 16 bytes
```

---

#### GCC Attributes for Control  
1. **Packed**: Remove all padding (may cause unaligned access penalties).  
2. **Aligned**: Enforce alignment.  

```c
struct __attribute__((packed)) picture {
    char height;     // 1 byte
    int **data;      // 8 bytes (no padding)
    int width;       // 4 bytes
    char *encoding;  // 8 bytes
}; // Size: 21 bytes (1 + 8 + 4 + 8)

struct __attribute__((packed, aligned(4))) picture2 {
    // Same members, but aligned to 4-byte boundary
}; // Size: 24 bytes (21 rounded up)
```

---

#### Flexible Array Members (C99+)  
- **Zero-length array** at the end of a struct for dynamic sizing.  

```c
struct array {
    int size;
    char data[]; // Flexible array member
};

int main() {
    struct array *var = malloc(sizeof(*var) + 32 * sizeof(char));
    var->size = 32;
    var->data[31] = 2; // Safe access
    printf("Size: %lu\n", sizeof(struct array)); // Output: 4 (size only)
}
```

---


### Example Demo: File Handling and Bitmap Processing

Read a bmp file and put a frame around the image

---

<table>
<tr><th>Input image</th>
<th>Output image</th></tr>
<tr><td>
<img src="./images/image-1.png" style="width:400px">
</td>
<td>
<img src="./images/image-2.png" style="width:400px">
</td></tr>
</table>

---

#### Streams and File I/O  
- **Default Streams**: `stdin`, `stdout`, `stderr`  
- **Functions**:  
  ```c
  FILE *f = fopen("file.bmp", "rb"); // Read binary
  fclose(f);
  fread(buffer, size, count, file); // Binary read
  fwrite(buffer, size, count, file); // Binary write
  fseek(f, offset, SEEK_SET); // Reposition file pointer
  ```

---

#### Bitmap File Structure (BMP)
[see wikipedia](https://en.wikipedia.org/wiki/BMP_file_format)
1. **BMP Header**:  
   - File type (`BM`), size, offset to pixel data.  
2. **DIB Header**:  
   - Width, height, bits per pixel (e.g., 32-bit ARGB).  
3. **Pixel Data**:  
   - Stored as BGRX (little-endian, 4 bytes per pixel).  


---

Example code to read headers:  

```c
FILE *f = fopen("image.bmp", "rb");
uint32_t offset, hsize, width, height;
uint16_t nplanes, nbits;
fseek(f, 10, SEEK_CUR);  /* skip 10 bytes   */
fread(&offset, 4, 1, f); /* data offset     */
fread(&hsize, 4, 1, f);  /* DIP header size */
fread(&width, 4, 1, f);
fread(&height, 4, 1, f);
fread(&nplanes, 2, 1, f);
fread(&nbits, 2, 1, f);
printf("offset: %d, hsize: %d, \n"
  "width: %d, height: %d, nplanes: %d, bits: %d",
  offset, hsize, width, height, nplanes, nbits);
fclose(f);
```

---

### Copying an image header

```C
/*storage for the data upto offset of image data*/
uint32_t *fheader = (uint32_t *) malloc(sizeof(uint8_t)*offset);

/*rewind to the beginning*/
rewind(infile);

/*read and write the header upto offset*/
fread(fheader, 1, offset, infile);
fwrite(fheader, 1, offset, outfile);
```

---

#### Pixel Manipulation  
- **BGRX Format**:  
  ```c
  typedef union {
      uint32_t pixel;
      struct {
          uint8_t b; // Blue
          uint8_t g; // Green
          uint8_t r; // Red
          uint8_t a; // Alpha (unused in 32-bit BMP)
      } __attribute__((packed));
  } Pixel;
  ```

---

- **Example**: Add a frame:  
```c
Pixel p;
uint32_t FRAME_COLOR = 0x000000FF; /*a=0, r=0, g=0, b=FF*/
//uint32_t FRAME_COLOR = 255<<16 | 255<<8 | 255; /*(little endian)*/

rewind(infile);
fseek(infile,offset,SEEK_CUR);/*go to image data*/
for (int i = 0; i < height; i++) {
    for (int j = 0; j < width; j++) {
      fread(&p.pixel, 4, 1, infile);
      if (j < 50 || i < 50 || i > height-50 || j > width-50) 
        fwrite(&FRAME_COLOR, 4, 1, outfile);
      else 
        fwrite(&p.pixel, 4, 1, outfile);
    }
}
```

---

#### Bit Fields and Endianness  
- **Bitfield Struct**:  
  ```c
  struct pixel {
      unsigned int b : 8; // Little-endian: `b` stored first
      unsigned int g : 8;
      unsigned int r : 8;
      unsigned int a : 8;
  };
  ```
- **Union for Direct Access**:  
  ```c
  union {
      uint32_t value;
      struct pixel components;
  } pixel_data;
  pixel_data.value = 0x12345678; 
  // b=0x78, g=0x56, r=0x34, a=0x12 (little-endian)
  ```

---

### Error Handling in C

---

#### Using `errno`, `strerror`, and `perror`
- **`errno`**: Global variable set by system calls/library functions to indicate errors.  
- **`strerror(errno)`**: Converts `errno` to a human-readable string.  
- **`perror("message")`**: Prints `message: error_string` to `stderr`.  

---

Example:  
```c
#include <stdio.h>
#include <errno.h>
#include <string.h>

int main() {
    FILE *f = fopen("/nonexistent/file.txt", "r");
    if (f == NULL) {
        fprintf(stderr, "Error code: %d\n", errno);
        fprintf(stderr, "Error message: %s\n", strerror(errno));
        perror("Failed to open file");
    }
    return 0;
}
// Output:
// Error code: 2
// Error message: No such file or directory
// Failed to open file: No such file or directory
```

---

### Common Bugs in C Programs

---

#### Dangling Pointers and Double Free  
```c
int *p = malloc(sizeof(int));
free(p);
*p = 123; // Dangling pointer: writing to freed memory
free(p);  // Double free: undefined behavior
```

**Fix**: Set pointers to `NULL` after freeing.  
```c
free(p);
p = NULL;
```

---

#### Buffer Overflow/Underflow  
```c
#define N 10
int array[N];
for (int i = N; i >= 0; i--) // i >= 0 → accesses array[-1]
    array[i] = i;
```

**Fix**:  
```c
for (int i = N-1; i >= 0; i--)
    array[i] = i;
```

---

#### Uninitialized Variables  
```c
int x;
int y = x + 2; // Undefined behavior (x is garbage)
```

**Fix**: Always initialize variables.  
```c
int x = 0;
int y = x + 2;
```

---

### Debugging with GDB

---

#### Basic Commands  
1. **Compile with Debug Symbols**:  
   ```bash
   gcc -g -o program program.c
   ```
2. **Start GDB**:  
   ```bash
   gdb ./program
   ```
3. **Key Commands**:  
   - `break main`: Set breakpoint at `main`.  
   - `run`: Start execution.  
   - `next`: Execute next line (step over functions).  
   - `step`: Step into functions.  
   - `print x`: Print value of variable `x`.  
   - `backtrace`: Show call stack on crash.  

---

#### Example Debugging Session  
```c
#include <stdio.h>
int main() {
    int val = 1;
    val = 42;
    asm("int $3"); // Trigger breakpoint manually
    val = 7;
    return 0;
}
```

**GDB Output**:  
```bash
(gdb) run
Program received signal SIGTRAP, Trace/breakpoint trap.
main () at program.c:5
5     val = 7;
(gdb) print val
$1 = 42
```

---

### Memory Leak Detection with Valgrind

---

#### Usage  
1. **Compile with `-g`**:  
   ```bash
   gcc -g -o program program.c
   ```
2. **Run Valgrind**:  
   ```bash
   valgrind --leak-check=full ./program
   ```

---

#### Example Output for a Leak:  
```bash
==12345== 40 bytes in 1 blocks are definitely lost in loss record 1 of 1
==12345==    at 0x483B7F3: malloc (vg_replace_malloc.c:307)
==12345==    by 0x1091FE: main (program.c:4)
```

**Fix**: Ensure every `malloc` has a corresponding `free`.  

---

### System Calls and Kernel Interaction

---

#### System Calls vs. C Functions  
- **System Calls**: Direct requests to the kernel (e.g., `open`, `write`).  
- **C Functions**: Wrappers around syscalls (e.g., `fopen`, `fwrite`).  
- Example:  
  ```c
  // System call (Linux x86-64 assembly)
  mov $1, %rax    // Syscall ID for `write`
  mov $1, %rdi    // File descriptor (stdout)
  mov $msg, %rsi  // Buffer
  mov $12, %rdx   // Length
  syscall
  ```  


---

#### Transition to Kernel Mode  
- **System Calls**: Triggered via software interrupts (e.g., `int 0x80` on x86, `syscall` on x86-64).  
- **Privileged Operations**: File I/O, process creation, memory allocation.  

Example (`write` syscall in x86-64 assembly):  
```assembly
mov $1, %rax    // Syscall ID for write
mov $1, %rdi    // File descriptor (stdout)
mov $msg, %rsi  // Buffer address
mov $12, %rdx   // Buffer length
syscall
```

---

*Example*
```C
#include <unistd.h>
int main(int argc, char *argv[]){
    write(1, "Hello World\n", 12); /* write "Hello World" to stdout */
    _exit(0); /* exit with error code 0 (no error) */
}
```
x86-64 equivalent assembly code
```assembly
_start:
movq $1, %rax   //use the `write` [fast] syscall
movq $1, %rdi   //write to stdout
movq $msg, %rsi // use string "Hello World"
movq $12, %rdx  // write 12 characters
syscall         // make syscall

movq $60, %rax  // use the `_exit` [fast] syscall
movq $0, %rdi   // error code 0
syscall         // make syscall
```

---

#### System Calls: Key Points to Remember:
- User Mode → Kernel Mode transition
- Error handling via errno
- Expensive context switches

---

### Related terms: Interrupts, Exceptions, and Traps

| **Type**         | **Description**                                                                 |
|-------------------|---------------------------------------------------------------------------------|
| **Interrupt**     | Asynchronous (e.g., hardware events like keyboard input).                      |
| **Exception**     | Synchronous (e.g., division by zero, page fault).                              |
| **Trap**          | Intentional (e.g., `int 0x80` to invoke a system call).                        |

---

### Best Practices

1. **Avoid System Call Overhead**: Batch operations (e.g., use buffered I/O with `fwrite` instead of `write`). 
   - see `man setvbuf` 
2. **Check Return Values**: Always validate system call/library function results.  
3. **Secure Coding**:  
   - Use `fgets` instead of `gets`.  
   - Validate input sizes to prevent buffer overflows.  
   - Initialize pointers to `NULL` and variables to default values.  

---
