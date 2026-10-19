# TinyShell

a small shell in C, written to be read.

it runs external commands and keeps two builtins. everything else is missing
on purpose. each missing piece is a step in the roadmap below, ordered so that
every step builds on the one before it.

after a few evenings with this you should be able to explain why
`ls | grep foo` needs two processes, why `cd` can never be an external
program, and what `$?` is actually holding.

```
$ make run
> echo hello
hello
> exit 7
$ echo $?
7
```

## what a shell actually does

the entire program is a loop, and the loop has five stages:

```
   read  ->  tokenize  ->  expand  ->  execute  ->  remember $?
```

| stage | what it means | where |
|---|---|---|
| **read** | get one line of text, deal with EOF and with lines that do not fit | `read_line()` in `src/tinyshell.c` |
| **tokenize** | turn text into a `char *argv[]`, which is what a C program expects to find in `argv` | `src/parser.c` |
| **expand** | turn `$HOME`, `~`, `*`, `$?` into values | *not implemented yet, see step 7* |
| **execute** | run a builtin in this process, or fork and hand over to a program on `PATH` | `src/builtins.c`, `src/executor.c` |
| **remember** | keep the exit status around for `$?` | `last_status` in `src/tinyshell.c` |

the execute stage surprises people, so it is worth saying plainly: **a shell
runs *one* program and then gets out of the way.** everything else (pipes,
redirection, job control, `cd`) is a variation on how that program gets set up
before `exec`, or a question about what happens to the shell's own state
afterwards.

two rules fall out of that, and both are easy to get wrong:

- **builtins are consulted before the fork, never after.** once a child
  exists, a change it makes cannot come back to the shell.
- **the status has to be remembered.** a value that lives only inside the
  function that ran the command is already gone by the time the next line
  arrives.

## build and run

```sh
make            # prints the target list
make build      # compile to build/tinyshell
make run        # build and start it
make clean      # remove build/
```

what each target does, why the build is set up this way, and how to ask for a
sanitized build when you want one, is in
[INFRASTRUCTURE.md](INFRASTRUCTURE.md).

## reading the code

start here, in this order. the files are small and each one has one job:

| file | lines | what it teaches |
|---|---|---|
| `src/tinyshell.c` | ~150 | the REPL loop, and the shape every shell has |
| `src/parser.c` | ~90 | turning text into `argv[]` |
| `src/builtins.c` | ~105 | dispatch, and why some commands cannot be external |
| `src/executor.c` | ~100 | `fork`, `execvp`, `waitpid`, and exit statuses |

the comments carry most of the weight. they are not there to say what the next
line does, because the code already says that. they are there to say *why it
is written this way* and what breaks otherwise, which is the part you cannot
get from reading the code alone.

## the roadmap

each step is small enough to do in an evening, and each one changes how you
think about the step after it. do them in order.

### step 1: `cd`

the first builtin that does real work, and the clearest example of why
builtins exist at all. a child that calls `chdir()` changes its own directory
and then exits; the shell is the parent and does not move. **you cannot
delegate a change of the shell's own state to a process.**

you will need `man 2 chdir`, `getenv()` from `man 3 getenv`, and `man 7
environ`. the traps: `$HOME` may not be set and may not exist, `cd` with no
argument is the one case that looks somewhere other than `argv[1]`, and you
have to decide what `cd ..` from `/` should do.

### step 2: quoting and escaping

right now `echo "a b"` produces two arguments. fix it in `parser.c`.

the question to sit with: **where does quoting belong?** not in the tokenizer,
which has to be able to say "this word was quoted" so that step 7 knows not to
expand `$` inside it, and has to be able to say "this word was never quoted at
all". that suggests a token is no longer a `char *` but a pointer *plus* some
flags.

try it yourself: after implementing quotes, `echo '$HOME'` must print
`$HOME` and `echo "$HOME"` must print your home directory. bash gets that
right and it is not accidental.

### step 3: redirection

`>`, `>>`, `<`, and then `2>`.

this is where the "set the child's world up, then exec" idea gets concrete:
`open()` the file, `dup2()` it onto fd 1 or 2, then exec. the child should
never know a redirect happened.

you will need `man 2 open`, `man 2 dup2`, and `man 2 close`. the trap is
ordering. `echo hi > out > other` means something specific, and so does
`> out echo hi`. a `struct redirect` on the side is the usual shape.

### step 4: pipes

the headline feature, and much smaller than it looks.

`ls | grep foo` is: one `pipe()`, fork twice, `dup2()` the write end onto the
first child's fd 1 and the read end onto the second child's fd 0, exec both,
then close both ends *in the parent* and wait for both.

you will need `man 2 pipe`, `man 2 dup2`, and `man 2 read`. the traps: the
parent must close its copies or nothing ever sees EOF; `SIGPIPE` turns up the
moment someone pipes into `head`; and one line now produces several argv
arrays, which is the reason `parser.c` does not use `strtok()`.

### step 5: background jobs and `&`

fork, do not wait, print the pid, keep reading. then the shell needs a job
table, because something has to eventually collect those pids.

this is where `$!` and `wait` come from, and where `shell_step()` stops being
the whole story. you will need `man 2 waitpid` and `waitid()`.

### step 6: signals

today, Ctrl-C kills the shell itself, because the shell is in the foreground
process group along with whatever it is running. real shells put the child in
its own process group, ignore SIGINT in the shell itself, and reap the child
with `WIFSIGNALED`, which `executor.c` already handles.

you will need `man 2 setpgid`, `man 2 sigaction`, and `man 7 signal`. this is
where you learn that a shell owns terminal state that it has to hand back and
take again, and it is the one people skip and later wish they had not.

### step 7: expansion

now `last_status` in `src/tinyshell.c` finally gets used. `$?` is the
motivating example: it cannot be done in the tokenizer, because the tokenizer
does not know what ran last, and it cannot be done during execution, because
`exec` has already replaced the process.

then `$VAR`, `~`, and `*`. `*` is the interesting one. globbing means
`opendir()`, `readdir()`, `stat()`, and sorting, all before anything runs.

### step 8: job control, if you are still going

`fg`, `bg`, `Ctrl-Z`, and the terminal (`man tty`, `tcsetpgrp()`). this is
where shells earn their reputation for complexity, and it is genuinely
optional.

## the syscalls you need

nearly all of "how does a shell work" reduces to this table:

| shell feature | syscall | read this |
|---|---|---|
| run a program | `fork` `execvp` `waitpid` | `man 2` |
| find a program | `PATH` lookup | `man 3 execvp`, `man 7 environ` |
| change directory | `chdir` `getcwd` | `man 2` |
| `a > b` | `open` `dup2` `close` | `man 2` |
| `a \| b` | `pipe` `dup2` | `man 2` |
| `a &` | `fork` + no wait | `man 2` |
| exit status | `waitpid` status word | `man 2 waitpid`, `WEXITSTATUS` |
| Ctrl-C | `setpgid` `sigaction` | `man 2`, `man 7 signal` |
| terminal | `tcsetpgrp` | `man tty`, `man tcsetpgrp` |

learn to read a man page. `man 2 fork` and `man 3 execvp` will answer most
questions you end up with in this repo, and they are shorter than you expect.

one trap to know about early: the section number in `man 2 chdir` is not always
2. glibc and the BSDs disagree about a handful of pages. `tcsetpgrp` is section
3 on Linux and 2 on macOS, and the controlling terminal is `man 4 tty` on Linux
and `man 7 tty` on BSD. when `man` says "no manual entry", drop the section
number and let it search.

## conventions

- `ts_` prefixes everything, because `fork`, `open` and friends all live in
  the same namespace and a shell grows names quickly.
- two spaces of indentation, 80 columns, no tabs in C. run
  `clang-format -i src/*.c` before you commit; `.clang-format` has the details.
- comments explain *why*. if a comment could be deleted without anyone losing
  information, it should be deleted.
- a command that fails prints to stderr and lets the loop continue. a shell
  that exits because one command failed is not a shell.

## license

MIT. see [LICENSE](LICENSE).