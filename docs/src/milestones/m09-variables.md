# M9: environment variables

do M8 first. this milestone touches three files: `src/expand.c`, the
`MILESTONE 9` comment in `src/parser.c`, and the `export`/`unset` stubs in
`src/builtins.c`. `man 7 environ` is short and worth a look.

```
> export GREETING=hello
> echo $GREETING world
hello world
> printenv GREETING
hello
> false
> echo $?
1
> echo '$HOME' "$HOME"
$HOME /home/you
> unset GREETING
> echo [$GREETING]
[]
```

## the environment

every process carries a list of `NAME=value` strings. a child gets a copy
of its parent's through `fork()`, and keeps it through `exec()`.
`getenv()`, `setenv()` and `unsetenv()` work on the current process's copy.

which means `export` has to be a builtin, for the same reason `cd` does. a
child calling `setenv()` would only change its own copy, and that copy
disappears when it exits. once the shell itself calls `setenv()`, every
program it starts from then on inherits the variable automatically.

## where you're starting from

the shell already records the status of every command with
`ts_expand_set_last_status()`, and `ts_expand_variable("?", 1)` returns it
as text. `export`, `unset` and `history` are in the builtin table but only
print "not implemented".

## what to write

start with `ts_expand_variable()`. the name you get isn't NUL-terminated;
it points into the middle of the input line. copy it into a local buffer
(check the length first, a 5000-character name can't overflow a 256-byte
buffer) and call `getenv()`. an unset variable expands to `""`.

then the lexer. when you see `$` outside single quotes, read the name that
follows: either `?`, or a run of letters, digits and underscores. look it
up and feed the value through `append_char()` one byte at a time. a `$`
with nothing usable after it is just a `$`.

the value can be much longer than the name, which is why tokens get their
own buffer, twice the size of a line, and why every byte goes through the
bounds check.

for `export NAME=value`, find the first `=` with `strchr()`, copy the name
into a buffer, and call `setenv(name, value, 1)`. check the return value.
`unset NAME` is a call to `unsetenv()`.

don't write into the string `getenv()` returns. it's the environment's own
memory.

## questions to try

1. after `export X=1`, how does `printenv` see `X` when the shell never
   passed it explicitly?
2. why does expansion happen in the lexer rather than the parser?
3. you run `export X=1` inside TinyShell. is `X` set in the terminal you
   started TinyShell from?

## if you want more

`${NAME}` so that `${HOME}dir` works, `X=1` assignments that don't export,
`~` at the start of a word, `$$` for the shell's pid and `$!` for the last
background job.
