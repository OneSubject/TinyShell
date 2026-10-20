# learning with this repo: quick reference

the full explanation is in the book: [the milestone
workflow](docs/src/workflow.md), [tools](docs/src/appendix/tools.md) and
[writing tests](docs/src/appendix/writing-tests.md). this page is the
commands on one screen, for when you already know the loop.

## each milestone

```sh
make book-serve                  # 1. read the milestone's chapter
grep -rn "TODO(M4)" src/         # 2. find the spot
make test MILESTONE=4            # 3. see what fails
make run                         # 4. build it, try it by hand
make test MILESTONE=4            # 5. make it pass...
make test-debug MILESTONE=4      #    ...under the sanitizers too
```

6. set `MILESTONE ?= 4` in the `Makefile`, tick the box in `README.md`,
   commit.

## test results

| | |
|---|---|
| `PASS` | works |
| `FAIL` | a milestone at or below `MILESTONE` is broken |
| `TODO` | a later milestone, not expected yet |
| `PASS (ahead of ...)` | a later milestone's test already passes |

## when stuck

```sh
man 2 dup2                         # ERRORS and NOTES sections
make debug                         # sanitizers name the bad line
make gdb                           # step through; follow-fork-mode child
strace -f build/tinyshell          # every fork, dup2, close, execve
sh -c 'your command'               # what the answer should be
```

## before committing

```sh
make fmt && make lint && make clean test && make test-debug
```

## adding things

- a builtin: a function in `src/builtins.c`, one line in `builtins[]`.
- a test: a function in `tests/run_tests.sh` starting with
  `milestone N "..."`, added to the list at the bottom.
- a source file: put it in `src/`. the Makefile finds it.
- a book chapter: a `.md` in `docs/src/`, and a line in
  `docs/src/SUMMARY.md`.
