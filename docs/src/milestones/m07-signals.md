# M7: signals

everything for this one is in `src/signals.c`. read the
[signals](../concepts/signals.md) chapter before starting; most of the
difficulty here is knowing the rules, not writing the code.

```
> sleep 100
^C
> echo still here
still here
> ^C
>
```

## where you're starting from

the plumbing is already in place. `main()` calls `ts_signals_install()`
once. `exec_child()` calls `ts_signals_reset_for_child()` between fork and
exec. `read_line()` treats an interrupted read (`EINTR`) as "give me a new
prompt", and `wait_for_child()` retries `waitpid()` when it's interrupted.
`ts_jobs_reap()` checks `ts_signals_take_sigchld()` before doing anything.

all three functions in `signals.c` are empty.

## what to write

start with SIGINT. write a handler that does nothing,
`static void on_sigint(int signo) { (void)signo; }`, and install it with
`sigaction()`. leave `SA_RESTART` out of the flags, so that pressing Ctrl-C
at the prompt interrupts `fgets()` and you get a fresh line.

then fill in `ts_signals_reset_for_child()` so it sets SIGINT back to
`SIG_DFL`. without this step your shell survives Ctrl-C and so does every
program it runs.

if you've done M6, add SIGCHLD too. the handler only sets a flag, a
`static volatile sig_atomic_t` that starts at 1 so the first check runs.
install it with `SA_RESTART | SA_NOCLDSTOP`. `ts_signals_take_sigchld()`
returns the flag and clears it.

you'll need `<signal.h>`, and `<string.h>` for `memset()`.

## mistakes people make

using `signal()` instead of `sigaction()`. glibc's `signal()` only keeps
the handler installed when `_DEFAULT_SOURCE` is defined, and the Makefile's
`-std=c11 -D_POSIX_C_SOURCE=200809L` means it isn't. so in this project the
handler is removed after it runs once, and the second Ctrl-C kills the
shell. `man 2 signal` has the details.

using `SIG_IGN` and skipping the reset. an ignored signal stays ignored
through `exec`, so every program the shell starts can't be stopped with
Ctrl-C. the "SIGINT still kills the foreground program" test is there for
this.

calling `printf()` in a handler. it isn't async-signal-safe: if the signal
lands while the main program is already inside `printf()`, the second call
can scramble its buffer or deadlock. `write()` is allowed, but an empty
handler is simpler.

calling `waitpid(-1, ...)` in the SIGCHLD handler. it can reap the
foreground child before `wait_for_child()` does, and then the shell's own
`waitpid()` fails with `ECHILD` and the exit status is gone.

once the tests pass, try it in a terminal: Ctrl-C during `sleep 100`
should bring the prompt back, Ctrl-C at the prompt should give a new line,
and after M9 `echo $?` after the interrupted sleep should print 130.

## questions to try

1. which processes does the terminal send SIGINT to?
2. exec resets a caught signal but not an ignored one. why?
3. why does the SIGCHLD handler only set a flag?

## if you want more

Ctrl-\ sends SIGQUIT, which deserves the same treatment. the proper fix is
job control, where each job gets its own process group and the shell
never receives Ctrl-C in the first place; see
[job control](../concepts/job-control.md).
