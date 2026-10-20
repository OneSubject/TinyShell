# Appendix A: The syscalls, at a glance

nearly all of "how does a shell work" fits in this table.

| shell feature | calls | milestone | read |
|---|---|---|---|
| run a program | `fork` `execv` `waitpid` `_exit` | skeleton | `man 2 fork`, `man 3 exec`, `man 2 waitpid` |
| change directory | `chdir` `getenv` | M1 | `man 2 chdir`, `man 3 getenv` |
| parse a number | `strtol` | M2 | `man 3 strtol` |
| find a program | `getenv` `stat` `access` | M3 | `man 2 stat`, `man 2 access` |
| `a > b` | `open` `dup2` `close` | M4 | `man 2 open`, `man 2 dup2` |
| `a \| b` | `pipe` `dup2` `close` | M5 | `man 2 pipe`, `man 7 pipe` |
| `a &` | `fork`, `waitpid(..., WNOHANG)` | M6 | `man 2 waitpid` |
| Ctrl-C | `sigaction` `sigemptyset` | M7 | `man 2 sigaction`, `man 7 signal`, `man 7 signal-safety` |
| variables | `getenv` `setenv` `unsetenv` | M9 | `man 3 setenv`, `man 7 environ` |
| is it a terminal? | `isatty` | skeleton | `man 3 isatty` |
| job control | `setpgid` `tcsetpgrp` `kill` | beyond | `man 2 setpgid`, `man 3 tcsetpgrp` |

## return value conventions

| family | success | failure |
|---|---|---|
| system calls (`open`, `fork`, `dup2`, ...) | `>= 0` | `-1`, reason in `errno` |
| `fork` | child pid in parent, `0` in child | `-1` |
| `exec*` | never returns | `-1` |
| `waitpid(..., WNOHANG)` | pid, or `0` if nothing finished | `-1` |
| `getenv` | pointer (do not modify) | `NULL` if unset |
| `snprintf` | length the result *wanted* | negative on encoding error |
| `strtol` | the number | check `errno`, the end pointer, and `*end` |

`errno` is only meaningful right after a call reports failure. successful
calls may change it too, so read it immediately.

## man page sections

| section | contents |
|---|---|
| 1 | commands (`man 1 ls`) |
| 2 | system calls: the kernel interface |
| 3 | C library functions |
| 7 | overviews (`man 7 signal`, `man 7 pipe`) |

if `man 2 fork` says there's no manual entry, install `manpages-dev`. if
you're not sure which section something is in, `man -k tcsetpgrp` will
tell you (it's 3).
