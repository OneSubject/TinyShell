# Appendix B: Tools

every target in the Makefile, and what to reach for when.
`make` with no arguments prints this list.
`INFRASTRUCTURE.md` in the repository root explains the reasoning behind
the setup.

| command | does |
|---|---|
| `make build` | compile to `build/tinyshell` |
| `make run` | build and start the shell |
| `make test` | unit tests + shell tests, up to `MILESTONE` |
| `make test MILESTONE=5` | the same, holding you to milestone 5 |
| `make test-unit` | only `tests/unit_tests.c` |
| `make test-shell` | only `tests/run_tests.sh` |
| `make test-debug` | the shell tests against the sanitized build |
| `make debug` | run the shell with AddressSanitizer + UBSan |
| `make gdb` | the sanitized shell inside gdb |
| `make lint` | static analysis with cppcheck |
| `make fmt` | format all C files with clang-format |
| `make fmt-check` | report formatting problems, change nothing |
| `make book` | build this book into `build/book/` |
| `make book-serve` | serve it at http://localhost:3000, rebuilding on save |
| `make install` | copy to `~/.local/bin` (`PREFIX=/usr/local` for system-wide) |
| `make clean` | remove `build/` |
| `make V=1 ...` | show the full compiler command lines |

## sanitizers

`make debug` and `make test-debug` use a separate build in `build/debug/`,
compiled with `-fsanitize=address,undefined`. they catch, at the moment it
happens:

- writing or reading past the end of an array
- use after free, double free
- memory leaks (reported at exit)
- signed integer overflow, invalid shifts, misaligned pointers

a sanitizer report names the file and line. read the first stack trace;
the rest is usually consequence.

## gdb

```
$ make gdb
(gdb) break exec_child
(gdb) run
> ls
Breakpoint 1, exec_child (...)
(gdb) print command->argv[0]
(gdb) bt
```

useful for a forking program:

| command | effect |
|---|---|
| `set follow-fork-mode child` | follow the child after `fork()` |
| `set detach-on-fork off` | keep both processes under gdb |
| `info inferiors` | list them |
| `catch exec` | stop when a process calls exec |

leak checking is switched off under gdb (`ASAN_OPTIONS=detect_leaks=0`)
because LeakSanitizer cannot run under ptrace.

## strace

the fastest way to understand a pipe or redirection bug:

```sh
$ strace -f -e trace=fork,clone,execve,pipe2,dup2,close,wait4 \
    build/tinyshell
```

`-f` follows children. every line starts with the pid, so you can see which
process closed (or failed to close) which fd.

## /proc

```sh
$ ls -l /proc/<pid>/fd       # open fds of a process
$ cat /proc/<pid>/status     # state, parent, signal masks
$ ps -o pid,ppid,pgid,stat,comm
```
