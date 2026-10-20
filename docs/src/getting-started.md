# Setting up

TinyShell is for Linux only. the tests read `/proc`, the book's examples
assume glibc and the GNU tools, and the Makefile stops with an error on
anything else. you need `gcc` (or `clang`) and `make`; `gdb`, `cppcheck`,
`clang-format` and `strace` are optional but you'll want them eventually.
on Debian or Ubuntu:

```sh
sudo apt install build-essential manpages-dev gdb cppcheck clang-format strace
```

`manpages-dev` is the one people miss. without it, `man 2 fork` says
there's no manual entry, and you'll be reading the man pages a lot.

to build this book as a website you also need `cargo install mdbook
mdbook-mermaid`. that part is optional; the markdown reads fine without
it.

## building

```sh
$ make            # lists the targets and does nothing else
$ make build      # compiles to build/tinyshell
$ make run        # builds, then starts the shell
> echo hello
hello
> cd /tmp
> pwd
/tmp
> exit
```

Ctrl-D at an empty prompt also quits.

## testing

```sh
$ make test
```

on a fresh clone the output ends with something like:

```
  PASS  M3  a file without the x bit reports 126
  TODO  M4  > sends stdout to a file, and only for that command
  ...
19 passed, 0 failed, 22 todo  (MILESTONE=3)
```

milestones 0 to 3 pass and everything after is `TODO`. if anything says
`FAIL` before you've changed a line, your system differs from what the
tests assume, and it's worth figuring out why before you start.

## what's where

```
TinyShell/
├── src/          the shell, one .c and .h per job
├── tests/        unit_tests.c and run_tests.sh
├── docs/         this book
├── Makefile      run `make` to see the targets
├── README.md     short overview and the roadmap
└── INFRASTRUCTURE.md   why the build is set up the way it is
```
