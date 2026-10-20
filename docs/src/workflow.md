# The milestone workflow

this is the routine for each milestone. you don't have to follow it, but
the tests and Makefile targets are set up around it.

first read the milestone's chapter, and the concept chapter it points you
to. then find the code:

```sh
$ grep -rn "TODO(M4)" src/
```

next, see what's failing. `make test` only holds you to milestones up to
`MILESTONE`, which starts at 3. bump it for one run:

```sh
$ make test MILESTONE=4
```

the M4 tests change from `TODO` to `FAIL`, and each one shows what it
expected and what it actually got:

```
  FAIL  M4  >> appends instead of truncating
      stdout was:
          |
      expected:
          | one
          | two
```

that's your to-do list. work through it in small steps, and try things by
hand with `make run` as you go (`echo hi > /tmp/x`, then `cat /tmp/x`).
small steps make it obvious which change broke what.

when the tests pass, run them again against the sanitized build:

```sh
$ make test MILESTONE=4
$ make test-debug MILESTONE=4
```

AddressSanitizer and UBSan catch overflows and undefined behaviour that a
normal build often gets away with.

then change `MILESTONE ?= 3` near the top of the `Makefile` to 4, so plain
`make test` keeps checking it, tick the box in `README.md`, have a go at
the questions at the end of the chapter, and commit.

## reading the results

`PASS` and `FAIL` mean what you'd expect. `TODO` is a test for a milestone
you haven't reached yet. occasionally you'll see `PASS (ahead of
MILESTONE=3)`: a later test that already passes. sometimes that's real, and
sometimes it's a test for a mistake you haven't had the chance to make yet
(the M7 Ctrl-C test is like that).

if something that used to pass starts failing, you've broken it, and the
test just saved you from finding out the hard way.

## when you're stuck

read the man page for the call you're using, especially the ERRORS and
NOTES sections.

add `fprintf(stderr, ...)` lines. use stderr, since stdout might be
redirected by the very feature you're working on.

run `make debug`. if you're corrupting memory, the sanitizer will usually
point at the exact line.

`strace -f -o trace.txt build/tinyshell` logs every `fork`, `dup2`, `close`
and `execve` in every process. for pipe bugs it's the quickest way in.

`make gdb` if you want to step through; the [tools](appendix/tools.md)
appendix has some gdb commands for forking programs.

and when you're not sure what the right behaviour is, `sh -c 'the command'`
will show you.
