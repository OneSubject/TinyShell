# M4: redirections

from here on the work is yours. this one happens in `ts_redirect_apply()`
in `src/redirect.c`. read the [file descriptors](../concepts/file-descriptors.md)
chapter first, then run `make test MILESTONE=4` to see what's failing.

```
> echo hello > out.txt
> echo again >> out.txt
> cat < out.txt
hello
again
> ls /nope 2> err.txt
> cat err.txt
ls: cannot access '/nope': No such file or directory
```

## where you're starting from

the parser already pulls redirects out of the command. for
`sort -r < in > out` you get:

```
argv      {"sort", "-r", NULL}
redirects [ {TS_REDIRECT_IN, "in"}, {TS_REDIRECT_OUT, "out"} ]
```

and `exec_child()` already calls `ts_redirect_apply()` at the right time,
in the child after `fork()` and before `execv()`. at the moment it prints
"not implemented yet" and gives up. you're writing the body.

## what to write

loop over `command->redirects` in order. for each one, work out the
`open()` flags and which fd it replaces:

| kind | flags | replaces |
|---|---|---|
| `TS_REDIRECT_IN` | `O_RDONLY` | `STDIN_FILENO` |
| `TS_REDIRECT_OUT` | `O_WRONLY \| O_CREAT \| O_TRUNC` | `STDOUT_FILENO` |
| `TS_REDIRECT_APPEND` | `O_WRONLY \| O_CREAT \| O_APPEND` | `STDOUT_FILENO` |
| `TS_REDIRECT_ERR` | `O_WRONLY \| O_CREAT \| O_TRUNC` | `STDERR_FILENO` |

then `open(path, flags, 0644)`, `dup2()` the new fd onto the target, and
`close()` the original. if `open()` or `dup2()` returns -1, report it with
`ts_error("%s", path)` and return -1. you'll need `<fcntl.h>` and
`<unistd.h>`.

## things that go wrong

the third argument to `open()` is easy to forget. with `O_CREAT` it's the
new file's permissions, and because `open()` is variadic the compiler
won't say a word if it's missing. the file just ends up with whatever
happened to be in that register.

if you skip the `close()`, the program you exec inherits an extra open fd.
nothing visibly breaks yet. in M5 the same mistake with a pipe makes
pipelines hang, so get in the habit now.

when `open()` fails, the command must not run. `cat < missing` should print
an error and that's it. returning -1 is enough; `exec_child()` calls
`_exit(1)` for you.

once fd 2 points at a file, any error message printed after that goes into
the file as well. that's correct, and bash does the same.

you're done when the M4 tests pass under both `make test MILESTONE=4` and
`make test-debug MILESTONE=4`. try `echo a > f > g` by hand too: bash
creates both files and only writes to `g`.

## questions to try

1. after `ls > out`, why doesn't the shell have to put stdout back?
2. what's the difference between `O_APPEND` and seeking to the end once
   after opening?
3. what would happen if `ts_redirect_apply()` ran in the parent?

## if you want more

for now TinyShell refuses `cd /tmp > out`, because a builtin runs in the
shell and there's no child to throw away afterwards. the end of the file
descriptors chapter shows how to save fd 1 with `dup()` and restore it.
after that, `2>&1`, `2>>` and here-documents (`<< EOF`) are all reasonable
next steps.
