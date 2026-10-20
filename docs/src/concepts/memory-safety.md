# memory safety in C

this matters everywhere, but you'll feel it most in
[M8](../milestones/m08-quoting.md) and [M9](../milestones/m09-variables.md),
when the lexer starts producing text whose length you can't predict from
the input.

C lets you write past the end of an array, read memory you've already
freed, and use variables you never initialised. none of that stops the
program at the time. it just behaves strangely later, usually somewhere
unrelated. below are the mistakes that come up most in a program like this
one, and what the TinyShell code does about each.

## 1. writing past the end of a buffer

```c
char name[16];
strcpy(name, user_input);   /* copies however much there is */
```

TinyShell uses fixed-size buffers, and every limit has a name in
`tinyshell.h`. in the lexer, the only function that writes into the token
buffer is `append_char()`, and it checks the size first. that way there's
one check to convince yourself of, instead of one at every write.

strings get built with `snprintf()` rather than `strcpy()`, `strcat()` or
`sprintf()`. it won't write past the size you give it, and it returns the
length the result wanted to be, so you can tell when something got cut off:

```c
int written = snprintf(out, size, "%s/%s", dir, name);
if (written < 0 || (size_t)written >= size)
  /* didn't fit: treat it as an error, don't use the truncated path */
```

`strncpy()` looks like the safe version of `strcpy()` and isn't. when the
source is too long, it leaves the destination without a `'\0'` at the end.

## 2. forgetting the terminator

a string of n characters takes n + 1 bytes. an `argv` with n arguments
takes n + 1 slots, because of the `NULL` at the end. both are accounted for
on purpose: `append_char()` checks against `used + 1`, and the lexer stops
at `TS_MAX_ARGS - 1` tokens.

## 3. not checking return values

```c
int fd = open(path, O_RDONLY);
dup2(fd, 0);   /* if open failed, fd is -1 and this fails too */
```

every system call in TinyShell has its result checked, and failures are
reported with `ts_error()`, which adds `strerror(errno)`. `errno` only
means something right after a call has reported failure, and the next
library call may overwrite it, so `ts_error()` saves it before doing
anything else.

## 4. writing to memory that isn't yours

the pointer `getenv()` returns points into the environment itself. if you
write to it (for example by running `strtok()` over it) you change `$PATH`
for the whole process. `path.c` walks it with two pointers and never
writes.

string literals are read-only. `char *s = "abc"; s[0] = 'x';` will
usually crash. it's one reason `ts_tokenize()` takes a `const char *` and
copies into its own buffer.

## 5. undefined behaviour in ordinary-looking code

`isspace(c)` with a negative `char` is undefined, and on x86-64 Linux
`char` is signed, so any non-ASCII byte is negative. cast first: `isspace((unsigned char)c)`.

`atoi("abc")` returns 0 and gives you no way of knowing it failed.
`strtol()` with an end pointer and `errno` does (see `builtin_exit()`).

signed overflow is undefined. `exit 99999999999999999999` goes through
`strtol()`, which reports `ERANGE` instead of overflowing.

passing the wrong type to a printf-style function is undefined too.
`TS_PRINTF_LIKE` in `error.h` gets the compiler to check every
`ts_error()` call the same way it checks `printf()`.

## 6. leaks and lifetimes

TinyShell mostly avoids `malloc()`. everything a line needs lives in local
variables in `shell_step()` and goes away when it returns, so there's
nothing to free and nothing to use after freeing. the trade-off is fixed
limits, which get reported as errors when you hit them.

if you do add `malloc()` (for unlimited history, say), make sure each
allocation has one clear owner and one place where it's freed, and run
`make test-debug`. LeakSanitizer lists anything still allocated at exit.

there's a lifetime rule hiding in the existing code: `ts_pipeline` holds
pointers into `ts_token_list.storage`, so the token list has to outlive the
pipeline. they're both locals in the same function, so it does.

## 7. file descriptors leak too

an fd left open after `dup2()`, or a pipe end the parent forgot to close,
won't crash anything. it'll make a pipeline hang instead (see
[pipes](pipes.md)). `ls -l /proc/<pid>/fd` shows what a process has open.

## tools

the compiler warnings (`-Wall -Wextra -Wpedantic`) are always on and catch
plenty. AddressSanitizer finds overflows, use-after-free and leaks; UBSan
finds signed overflow, bad shifts and misaligned access. you get both with
`make debug` and `make test-debug`. `make lint` runs cppcheck, which finds
some null dereferences and leaks the compiler misses. valgrind does much of
what ASan does without a rebuild: `valgrind ./build/tinyshell`.

the sanitizers make the program crash at the line that caused the problem,
instead of three commands later somewhere else. when something strange is
going on, try the sanitized build first.
