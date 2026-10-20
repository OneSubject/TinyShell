# infrastructure

what the build and tooling files are for, and why they are here.

if you just want to build and run the thing:

```sh
make            # prints every target
make run        # build and start the shell
make test       # check your work
```

everything below is the reasoning behind those commands. read it once when
you start, skip it after that. the per-target reference is in the book's
[tools appendix](docs/src/appendix/tools.md).

## the files

| file | job |
|---|---|
| `Makefile` | how the project is built, run, tested and checked |
| `tests/unit_tests.c` | unit tests for the lexer, parser and PATH lookup |
| `tests/run_tests.sh` | end-to-end tests that drive the real binary |
| `docs/book.toml`, `docs/src/` | the book, built with mdBook and mdbook-mermaid |
| `.clang-format` | formatting rules, enforced by a tool |
| `.editorconfig` | whitespace rules for editors that do not read clang-format |
| `.gitignore` | keeps build output, editor files and test debris out of git |
| `LICENSE` | MIT, so people can legally copy and modify this |

## the Makefile

### why a Makefile and not a shell script

you could write a `build.sh` that calls `cc` a dozen times, but you'd end
up reimplementing things make already does. it compares timestamps and
only rebuilds the `.o` files that are out of date. it knows that changing a
header means rebuilding every `.c` that includes it (see below). it can
build in parallel with `make -j8`, because the dependencies are written
down. and it's already installed.

### `make` with no arguments prints help

the default goal is `help`, not `build`, so someone new to the repo sees
the list of targets instead of having to guess them.

### incremental builds, and why `clean` is not a prerequisite

an earlier version of this Makefile had:

```make
build: clean $(TARGET)
```

which reads innocently and is a trap. it deletes `build/` before every
build, so nothing is ever incremental, and the workflow feels slow for
reasons nobody can name.

the rule to keep: **`clean` is something you ask for, not something a build
does to itself.** `make clean build` when you really want a fresh one.

### how header dependencies work

`cc -MMD -MP` writes a `.d` file next to each `.o`, listing what that object
depends on, and the Makefile does `-include $(DEPS)` to read them. `-MMD`
means "project headers only, skip `/usr/include`". `-MP` adds a phony rule
per header so that deleting one does not produce a confusing "No rule to
make target" error.

```
$ touch src/parser.h && make build
[CC] src/executor.c
[CC] src/parser.c
...
```

exactly the files that include `parser.h`, directly or indirectly, and no
list to maintain by hand.

### the POSIX feature test macro

```make
DEFS += -D_POSIX_C_SOURCE=200809L
```

`fork()`, `execv()` and `chdir()` are POSIX, not C11. `-std=c11` alone puts
the compiler in strict-ANSI mode, where every non-standard declaration is
hidden, and you get errors like `implicit declaration of function 'fork'`.
that is a confusing first hour, so the macro is set once, with a comment.

it has a side effect worth knowing about. defining `_POSIX_C_SOURCE`
yourself means glibc no longer defines `_DEFAULT_SOURCE`, and that changes
how some functions behave, not just which ones are declared. the one that
matters here is `signal()`, which goes back to resetting its handler after
one use. the M7 chapter covers it.

### Linux only

the top of the Makefile checks `uname -s` and stops on anything but Linux.
the code itself is mostly POSIX, but the tests read `/proc` and use GNU
`ps` and `env`, and the book describes glibc's behaviour. failing at the
first line beats failing halfway through the test suite with an error
nobody can explain.

### the warnings

`-Wall -Wextra -Wpedantic -Wshadow -Wstrict-prototypes`, and the rule is
zero warnings. `-Wpedantic` keeps the code honest C11. `-Wshadow` catches
an inner variable hiding an outer one, a classic source of "but I set it!".
`-Wstrict-prototypes` rejects `int f()`, which in C (unlike C++) means
"unknown arguments", not "no arguments".

### a separate directory for the debug build

`make debug`, `make gdb` and `make test-debug` use objects in
`build/debug/`, compiled with `-Og -g3 -fsanitize=address,undefined`. the
normal build lives in `build/obj/`.

they're kept apart on purpose. make decides what to rebuild from
timestamps, so if both builds shared one directory, switching flags would
leave old objects in place: a sanitizer-linked binary containing
unsanitized objects. it links fine and checks nothing. two directories make
that impossible, and switching between the builds costs nothing.

### `MILESTONE`

```make
MILESTONE ?= 3
```

the test runner tags each test with a milestone. tests up to `MILESTONE`
must pass; later ones run and report TODO. learners raise it as they go.
it is `?=` so `make test MILESTONE=5` works for one run without editing.

### overriding flags

`CFLAGS`, `DEFS`, `LDFLAGS`, `PREFIX`, `CC` are all `?=`, so a command line
assignment wins:

```sh
make clean build CC=clang          # does it build with the other compiler?
make install PREFIX=/usr/local     # system-wide
make V=1 build                     # print full compiler commands
```

build with the other compiler now and then. gcc and clang disagree about some POSIX declarations and warnings.

## the tests

two kinds, because a shell has two kinds of code.

**plain functions** (lexer, parser, PATH lookup) are tested in C, by calling
them. `tests/unit_tests.c` has no framework: a `CHECK()` macro and a
counter. a dependency would be one more thing to install before the first
`make test`, and the whole framework is fifteen lines a learner can read.

**everything that needs a process** (fork, pipes, signals, exit statuses) is
tested by `tests/run_tests.sh`, which pipes lines into the real binary and
checks stdout, stderr and the exit status. it is POSIX `sh`, so it runs
anywhere `make` does. every run is wrapped in `timeout`, because the most
common pipe bug is a hang, and a test suite that hangs teaches nothing.

the prompt only appears when stdin is a terminal (`isatty()`), which is
what keeps the shell's output clean enough to compare in tests.

## formatting and linting

`make fmt` runs clang-format; `make fmt-check` reports without changing
anything. `.clang-format` sets 2-space indent and 80 columns.
`ReflowComments: false` is the one setting that is about this project
rather than taste: the comments are prose and the line breaks in them are
deliberate, including the ASCII diagrams.

`make lint` runs cppcheck, which catches a different set of problems from
the compiler: null dereferences on some paths, leaks, suspicious format
strings.

both targets fail with an install hint if the tool is missing, rather than
silently doing nothing.

`.editorconfig` covers the rest: line endings, final newline, trailing
whitespace, and tabs in the `Makefile`.

## the book

`docs/` is an [mdBook](https://rust-lang.github.io/mdBook/) project, the
tool behind the Rust book. `make book` builds it into `build/book/`,
`make book-serve` serves it with live reload. mdBook is optional: every
chapter is plain markdown and reads fine on GitHub, starting from
`docs/src/SUMMARY.md`, which is also the table of contents.

chapters link to each other with relative `.md` paths, which work both in
mdBook and on GitHub.

the diagrams are [mermaid](https://mermaid.js.org/) code blocks, so they
live in the markdown as text and can be diffed and edited like everything
else. `mdbook-mermaid` turns them into pictures in the built book, and
GitHub renders them on its own. `docs/mermaid.min.js` and
`docs/mermaid-init.js` were generated by `mdbook-mermaid install docs` and
are committed so the book builds without a network connection. to update
mermaid, rerun that command.

## what's left out on purpose

there's no CI. it's one person on one machine, and
`make clean test test-debug` is the check. add CI when someone else starts
pushing.

there's no cmake. it's worth the trouble when you have dozens of targets,
several platforms or libraries to find; this is a dozen `.c` files and one
link step.

there's no test framework, for the reasons above, and no pre-commit hooks,
which new contributors don't know exist until one blocks them. the
checklist below is one line.

and there's no readline: it isn't POSIX, and line editing is a project of
its own (see M10).

## before you commit

```sh
make fmt && make lint && make clean test && make test-debug
```
