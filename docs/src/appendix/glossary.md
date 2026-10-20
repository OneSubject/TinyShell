# Appendix D: Glossary

**argv**: the `NULL`-terminated array of strings a program receives as
its arguments. `argv[0]` is the program's name.

**async-signal-safe**: a function that may be called from a signal
handler. a short list: `write`, `_exit`, `waitpid`, and a few dozen more.
`printf` and `malloc` are not on it.

**builtin**: a command the shell runs itself, without forking, because it
must change the shell's own state. `cd`, `exit`, `export`.

**child / parent**: after `fork()`, the new process is the child of the
one that called it.

**disposition**: what a process does when a signal arrives: the default
action, ignore it, or call a handler.

**environment**: a process's list of `NAME=value` strings, inherited by
its children.

**EOF**: end of file. on a terminal, Ctrl-D at the start of a line. for a
pipe, what the reader sees once every write end is closed.

**errno**: a global integer that system calls set to say why they failed.
only meaningful right after a failure.

**exit status**: the 8-bit number a process hands to its parent. 0 means
success. what `$?` holds.

**file descriptor (fd)**: a small integer naming an open file in a
process's fd table. 0, 1, 2 are stdin, stdout, stderr.

**foreground / background**: a foreground job owns the terminal and the
shell waits for it; a background job (`&`) runs while the shell keeps
reading.

**job**: one command line as the user sees it, which may be several
processes (a pipeline).

**lexer**: the part that turns characters into tokens.

**operator**: an unquoted `|`, `<`, `>`, `>>`, `2>` or `&`.

**orphan**: a process whose parent has exited; it is adopted by init.

**parser**: the part that turns tokens into structure (commands,
pipelines, redirects).

**PATH**: the colon-separated list of directories searched for a command
name.

**pid / pgid**: process id; process group id.

**pipe**: a kernel buffer with a write end and a read end.

**process group**: processes that receive terminal signals together, such
as all the parts of a pipeline.

**reap**: collect a finished child's exit status with `waitpid()`, freeing
its process table slot.

**REPL**: read, evaluate, print, loop: the shape of every interactive shell.

**session**: a set of process groups attached to one terminal.

**signal**: an asynchronous notification from the kernel, such as SIGINT.

**token**: one word or operator, as produced by the lexer.

**undefined behaviour**: code the C standard places no requirements on,
such as reading past an array. it may seem to work, until it does not.

**zombie**: a child that has exited but not been reaped. shown as `Z` in
`ps`.
