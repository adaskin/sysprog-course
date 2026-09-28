### 🛡️ Secure C Programming Rules for BIL 322 Labs

For all labs and assignments in BIL 322, you must follow these secure C programming practices. Code that violates these rules may lose points or fail to compile.

1. **No use of `gets()`.**  
   Always use `fgets()` instead.

   ```c
   char buf[64];

   if (!fgets(buf, sizeof(buf), stdin)) {
       /* handle error */
   }
   ```

   Note: `fgets` may keep the trailing newline character. Remove it if needed.

2. **No unbounded `scanf("%s", ...)`.**  
   If you must use `scanf` for strings, always specify a field width limit. The width must be one less than the buffer size because of the null terminator.

   ```c
   char name[16];

   scanf("%15s", name);
   ```

   Prefer `fgets()` for reading lines.

3. **Use `snprintf()` instead of `sprintf()`** for formatting into fixed-size buffers.

   ```c
   char buf[64];

   int n = snprintf(buf, sizeof(buf), "policy: %s", name);

   if (n < 0 || (size_t)n >= sizeof(buf)) {
       /* output was truncated or an error occurred */
   }
   ```

4. **Avoid `strcpy()` and `strcat()` unless the destination size is provably safe.**  
   Prefer safer alternatives such as `snprintf()`, bounded copying, or `strlcpy()` where available.

   If you use `strncpy()`, remember that it may not null-terminate the destination:

   ```c
   char dest[16];

   strncpy(dest, src, sizeof(dest) - 1);
   dest[sizeof(dest) - 1] = '\0';
   ```

5. **Always check return values** of system and library functions, including:

   - `fopen`
   - `fread` / `fwrite`
   - `malloc` / `calloc` / `realloc`
   - `fgets`
   - `sscanf`
   - `pthread_create`
   - `fork`
   - `open`
   - `read` / `write`

   For `realloc`, do not overwrite the original pointer directly:

   ```c
   void *tmp = realloc(ptr, new_size);

   if (!tmp) {
       /* handle error, original ptr is still valid */
   } else {
       ptr = tmp;
   }
   ```

6. **Ensure null-termination.**  
   When copying strings into fixed buffers, always ensure the destination has room for the null terminator and is explicitly terminated.

7. **Use the correct comparison function.**

   - Use `strcmp()` for exact comparison of valid null-terminated strings.
   - Use `strncmp()` only when comparing at most `n` characters intentionally, such as prefix matching.
   - Use `memcmp()` for binary headers, magic numbers, fixed-size fields, or non-null-terminated data.

   Do not blindly replace `strcmp()` with `strncmp()`.

8. **Initialize variables and structs.**  
   Do not read uninitialized memory.

   ```c
   int x = 0;
   char buf[64] = {0};
   ```

9. **Free allocated memory and do not use freed pointers.**  
   Every successful `malloc`, `calloc`, or `realloc` should eventually have a matching `free`, unless ownership is transferred elsewhere.

   Setting a pointer to `NULL` after freeing is a good habit:

   ```c
   free(p);
   p = NULL;
   ```

10. **Compile with strict warnings.**  
    Your code must compile cleanly with no warnings using flags such as:

    ```bash
    gcc -Wall -Wextra -Werror -std=c11 -g
    ```

    If you use POSIX functions such as `nanosleep`, `strdup`, `fileno`, `mmap`, or `fork`, you may need to either compile with:

    ```bash
    gcc -Wall -Wextra -Werror -std=gnu11 -g
    ```

    or define an appropriate feature macro at the very top of your source file before includes:

    ```c
    #define _POSIX_C_SOURCE 200809L
    ```

    For testing, you may also use sanitizers:

    ```bash
    gcc -Wall -Wextra -Werror -std=gnu11 -g -fsanitize=address,undefined
    ```

