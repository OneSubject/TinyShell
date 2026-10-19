# infrastructure

what the build and tooling files are for, and why they are here.

if you just want to build and run the thing:

```sh
make            # prints every target
make run        # build and start the shell
```

everything below is the reasoning behind those commands. read it once when
you start, skip it after that.

## the files

| file | job |
|---|---|
| `Makefile` | how the project is built and run |
| `.clang-format` | formatting rules, enforced by a tool |
| `.editorconfig` | whitespace rules for editors that do not read clang-format |
| `.gitignore` | keeps build output out of git |
| `LICENSE` | MIT, so people can legally copy and modify this |

## the Makefile

### why a Makefile and not a shell script

you could write `build.sh` that calls `cc` four times. for four files that
works fine. the reason to use `make` instead is that it already solves four
problems you would otherwise hand-roll badly:

- **incremental rebuilds.** make compares timestamps. it knows which `.o`
  files are older than their sources and only rebuilds those.
- **header dependency tracking.** a change to a `.h` file has to rebuild
  every `.c` that includes it. make discovers this relationship by itself,
  so nobody has to remember to maintain a list.
- **parallelism.** `make -j8` works because the dependency graph is declared.
  in a shell script you have to reason about ordering by hand.
- **it is already installed.** nothing to install, nothing to learn before
  your first build.

the dependency graph is also documentation. reading the top of the file tells
you the shape of the project without opening a single source file.

### the targets

| target | what it does |
|---|---|
| `make` | prints this list. the default goal is `help` |
| `make build` | compiles to `build/tinyshell` |
| `make run` | builds, then starts the shell |
| `make clean` | removes `build/` |

`make` with no arguments printing a helpful list instead of building is a
small thing, but it means a new contributor never has to guess the command
names. it is the first line of defence against a bad first five minutes.

### incremental builds, and why `clean` is not a prerequisite

this is the part worth internalising. an earlier version of this Makefile had:

```make
build: clean $(TARGET)
```

which reads innocently and is a trap. it deletes `build/` before every build,
so nothing is ever incremental. you save two seconds on a four-file project
and pay for it with a workflow that feels slow for reasons nobody can name.

the rule to keep: **`clean` is something you ask for, not something a build
does to itself.** `make clean build` when you really want a fresh one.

`make` also knows it does not need to rebuild things:

```
$ make build
make: Nothing to be done for 'build'.
```

### how header dependencies work

`cc -MMD -MP` writes a `.d` file next to each `.o`, listing what that object
depends on, and the Makefile does `-include $(DEPS)` to read them. `-MMD`
means "user headers only, skip the ones in `/usr/include`". `-MP` adds a
phony rule per header so that deleting one does not produce a confusing "No
rule to make target" error.

the effect, and the reason it is worth the two flags:

```
$ touch src/parser.h && make build
[CC] src/parser.c
[CC] src/tinyshell.c

$ touch src/builtins.h && make build
[CC] src/builtins.c
[CC] src/tinyshell.c
```

`parser.h` is included by `parser.c` and by `tinyshell.c`. `builtins.h` is
included by `builtins.c` and `tinyshell.c`. exactly the right work, and no
listing to maintain by hand.

### asking for different flags

`CFLAGS`, `DEFS` and `LDFLAGS` are all `?=`, so a command line assignment wins.
that is how you build a variant without the Makefile growing a target for it:

```sh
make clean build \
  CFLAGS="-std=c11 -O2 -g -Wall -Wextra -fsanitize=address,undefined" \
  LDFLAGS="-fsanitize=address,undefined"
```

that is a sanitized build. it needs two flags and no infrastructure, which is
why there is no dedicated target for it. the `make clean` is not optional:
make decides what to rebuild from timestamps, and the `.o` files from the last
build are already newer than the sources, so without the clean you get a
sanitizer-linked binary containing unsanitized objects. it links fine and
checks nothing.

keep `-O2` in that CFLAGS. the sanitizers instrument after the optimiser runs,
so a `-O0` build is a different program from the one you ship and its bugs are
not always yours.

worth reaching for when you are working on buffers, file descriptors or child
processes. a shell spends its life managing memory it does not own, and the
sanitizers say where you got it wrong without a debugger and without needing a
reproducible sequence.

### the POSIX feature test macro

```make
DEFS += -D_POSIX_C_SOURCE=200809L
```

`fork()`, `execvp()` and `chdir()` are POSIX, not C11. `-std=c11` alone puts
the compiler in strict-ANSI mode, where every non-standard declaration is
hidden, and you get errors like `implicit declaration of function 'fork'`.
That is a confusing first hour, so the macro is set once here with a
comment explaining it.

## formatting

`.clang-format` exists because reindenting code by feel produces enormous
diffs on small changes, and enormous diffs are where real changes hide. run:

```sh
clang-format -i src/*.c src/*.h
```

before you commit. the config sets 2-space indent and an 80-column limit to
match what is already in the tree, so it will not churn code you did not
touch.

`ReflowComments: false` is the one setting that is about this project
specifically rather than about taste. the comments in `src/` are prose and the
line breaks in them are deliberate, including the ASCII diagrams in
`executor.c`. letting the tool rewrap them would flatten the reasoning in
exactly the comments that exist to carry the reasoning.

`.editorconfig` covers the rest: line endings, final newline, trailing
whitespace, and tabs in the `Makefile`. it is what makes a fresh clone behave
the same in vim, emacs and VS Code without configuring each one.

## running the checks

there is no continuous integration here. the verification story is one local
command, which suits a repo this size and one maintainer:

```sh
make CC=clang build
```

building with the compiler you do not normally use earns its place. this
project leans on POSIX function declarations, and gcc and clang do not always
agree about what is implicitly declared or how strict they are about it. a
change that builds clean on your compiler and fails on the other is better
found by you than by the next person who touches the file.

if you only ever build with one compiler, that is the likeliest way for a
broken build to get committed.

## what is deliberately not here

knowing what was left out, and why, is as useful as knowing what is in.

- **no CI.** see the section above. one command you run yourself.
- **no tests.** you are not shipping this to anyone, and the interesting
  breakage in a shell is a segfault you can see on screen. a sanitized build
  (see above) is what catches the bad ones.
- **no cmake.** cmake earns its complexity at dozens of targets, several
  platforms, or external dependencies to locate. this is four `.c` files and
  one link step. a build system is a second thing to learn, and the point of
  this repo is to teach shell concepts.
- **no pre-commit hooks.** `clang-format` is one command. hooks are invisible
  to a new contributor until they break something.

## when you add something

worth adding, and what would justify it:

- **tests**, once the project has more than one person changing it, or once
  you want the gcc and clang builds both checked without remembering to.
  `shell_step()` will need unpacking from the REPL loop first, since it is
  `static` and prompts between commands.
- **CI**, at the same point, for the same reason.
- **cmake**, if the project grows past a handful of files or needs to find a
  library.
- **pre-commit hooks**, if `clang-format` starts getting skipped.

not worth adding on their own: more build targets, a second linting tool, or
any config for a problem this repo does not have yet.

## before you commit

```sh
clang-format -i src/*.c src/*.h   # formatting
make clean build                  # from scratch, no stale objects
```

add a sanitized build from the section above if you were working on buffers,
file descriptors or child processes. it is faster to fix a sanitizer report
than to read someone else's.