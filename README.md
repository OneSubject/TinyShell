# TinyShell

a small unix shell in C, written to be read, and then finished by you.

it already runs programs, finds them on `PATH`, and has `cd` and `exit`.
everything else (redirection, pipes, background jobs, Ctrl-C, quoting,
variables) is a milestone: a chapter in the book, a marked spot in the
code, and tests that turn from TODO to PASS as you build it.

after working through it you should be able to explain why `ls | grep foo`
needs two processes, why `cd` can never be an external program, why Ctrl-C
kills `sleep` but not the shell, and what `$?` is actually holding.

```
$ make run
> echo hello
hello
> cd /tmp
> pwd
/tmp
> exit 7
$ echo $?
7
```

## read the book

[the TinyShell book](docs/src/SUMMARY.md) is where everything is
explained: how a shell works, one chapter per milestone, one per systems
concept, and appendices for looking things up. it is built with
[mdBook](https://rust-lang.github.io/mdBook/), the tool behind the Rust
book, and its diagrams are drawn with mermaid:

```sh
cargo install mdbook mdbook-mermaid   # once
make book-serve     # opens http://localhost:3000, with sidebar and search
```

or read the markdown directly, starting at
[docs/src/SUMMARY.md](docs/src/SUMMARY.md).

start with [Introduction](docs/src/introduction.md) and the four
getting started chapters.

## quick start

TinyShell targets Linux only; the Makefile won't run anywhere else.

```sh
make              # list every target
make run          # build and start the shell
make test         # should end with "0 failed"
```

[LEARNING.md](LEARNING.md) has the per-milestone commands on one page.

## the roadmap

do them in order. each one is a few dozen lines, has its own tests, and
leaves the shell working. M1 to M3 are done, as worked examples of the
style the rest expect; their chapters walk through the code.

- [x] [M1: `cd`](docs/src/milestones/m01-cd.md): `chdir()`, the `$HOME` fallback, `errno`
- [x] [M2: `exit [n]`](docs/src/milestones/m02-exit.md): `strtol()` done right, 8-bit statuses
- [x] [M3: PATH lookup](docs/src/milestones/m03-path.md): `ts_find_in_path()`, `stat()`, 126 vs 127
- [ ] [M4: redirections](docs/src/milestones/m04-redirections.md): `>` `>>` `<` `2>` with `open()` and `dup2()`
- [ ] [M5: pipes](docs/src/milestones/m05-pipes.md): `a | b | c` with `pipe()`, closing every unused end
- [ ] [M6: background jobs](docs/src/milestones/m06-background-jobs.md): `&`, a job table, reaping zombies
- [ ] [M7: signals](docs/src/milestones/m07-signals.md): Ctrl-C stops the program, not the shell
- [ ] [M8: quoting](docs/src/milestones/m08-quoting.md): `'single'`, `"double"`, `\` in the lexer
- [ ] [M9: environment variables](docs/src/milestones/m09-variables.md): `export`, `unset`, `$VAR`, `$?`
- [ ] [M10: history](docs/src/milestones/m10-history.md) *(stretch)*: a ring buffer and a `history` builtin

`grep -rn "MILESTONE 4" src/` finds where each one starts.

## the layout

```
src/        the shell: one .c and .h per job (tinyshell.c has main)
tests/      unit_tests.c (C) and run_tests.sh (end to end)
docs/       the book: book.toml, src/SUMMARY.md, chapters
Makefile    build, run, test, debug, lint, fmt, book, install
```

why the build and tooling are set up the way they are:
[INFRASTRUCTURE.md](INFRASTRUCTURE.md).

## conventions

- `ts_` prefixes everything public. headers use `#pragma once`.
- each `.c` includes its own header first, then system headers, then other
  project headers ("include what you use").
- errors go through `ts_error()` / `ts_errorx()`, so every message looks
  the same.
- C11, `-Wall -Wextra -Wpedantic`, zero warnings. `make fmt` for layout.
- comments explain *why*. the code already says what.

## license

MIT. see [LICENSE](LICENSE).
