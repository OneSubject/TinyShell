# Introduction

this book is built around one small program, TinyShell, a unix shell
written in C. to make a shell work you have to deal with processes, file
descriptors, pipes and signals, and you have to be careful with memory,
because C won't be careful for you. that's what the book is really about.

you'll be building most of it yourself. the shell in this repository can
already run programs, find them on `PATH`, `cd` and `exit`. the rest is
split into ten milestones. each one has a chapter here, a marked spot in
the code (`MILESTONE N`, `TODO(MN)`), and tests that start out as `TODO`
and pass once you've done it.

by the end you should be able to explain why `ls | grep foo` takes two
processes, why `cd` can't be a separate program, why Ctrl-C stops `sleep`
but leaves the shell running, and what `$?` holds.

## before you start

you should be comfortable with basic C: pointers, arrays, structs,
`printf`, building with `make`. you should have used a terminal. you don't
need to have called `fork()` or `dup2()` before; that's what this is for.

## how it's laid out

the four chapters under *getting started* set up the project and explain
how a shell is put together. read them in order before anything else.

the milestones come next. M1 to M3 are already done, and their chapters
walk through the code so you can see what's expected. from M4 on, the code
is yours to write. go in order, since later ones build on earlier ones.

the concept chapters each explain one idea (processes, file descriptors,
pipes, and so on). every milestone tells you which one to read first, and
they work as reference on their own.

the appendices are for looking things up.

## reading it

it's plain markdown, so GitHub or any editor will show it fine; start at
[SUMMARY.md](SUMMARY.md). GitHub also draws the diagrams. to read it as a
website with a sidebar and search, install
[mdBook](https://rust-lang.github.io/mdBook/) and its mermaid plugin:

```sh
cargo install mdbook mdbook-mermaid
make book-serve     # then open http://localhost:3000
```

a few conventions: lines starting with `>` are typed at the TinyShell
prompt, lines starting with `$` are typed in your normal shell, and paths
like `src/parser.c` are relative to the repository root. `man 2 fork` means
"look at that manual page"; the number is the section (2 for system calls,
3 for library functions, 7 for overviews).
