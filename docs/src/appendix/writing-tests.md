# Appendix C: Writing tests

there are two kinds of test, and `make test` runs both.

## unit tests: `tests/unit_tests.c`

for code that is a plain function: the lexer, the parser, PATH lookup. they
call it directly and check the result with a `CHECK()` macro. no framework.

```c
static void test_tokenize_words(void) {
  ts_token_list tokens;

  CHECK(ts_tokenize("  ls   -l\t/tmp  ", &tokens) == TS_LEX_OK);
  CHECK(tokens.count == 3);
  CHECK_STR(tokens.tokens[0].text, "ls");
}
```

add a function, call it from `main()`, run `make test-unit`. a failing
check prints its file, line and expression, and the run continues so you
see every failure at once.

the test binary links every object in `build/obj/` except `tinyshell.o`
(which holds `main()`), so any public `ts_` function is testable.

## shell tests: `tests/run_tests.sh`

for everything that needs a running process: fork, pipes, signals. each
test pipes lines into `build/tinyshell` and checks what came out.

```sh
test_redirect_append() {
  milestone 4 ">> appends instead of truncating"
  run_shell <<EOF
echo one > $WORK/log
echo two >> $WORK/log
cat $WORK/log
EOF
  expect_stdout "one
two"
}
```

then add `test_redirect_append` to the list at the bottom of the file.

| helper | does |
|---|---|
| `milestone N "text"` | tags the test; it is TODO while `N > MILESTONE` |
| `run_shell [VAR=value ...]` | runs the shell, stdin from the here-document |
| `run_shell --no-home` | the same, with `HOME` unset |
| `expect_stdout "text"` | stdout must be exactly this |
| `expect_stdout_contains` / `expect_stdout_lacks` | substring checks |
| `expect_stderr_contains "text"` | substring of stderr |
| `expect_status N` | the shell's exit status (the last command's) |
| `fail "message"` | a custom failure |

the environment each test gets:

- `$WORK`: a scratch directory, also the current directory. deleted at the
  end.
- `$WORK/bin`, first on `PATH`, with small helper scripts: `ts-hello`,
  `ts-slow`, `ts-zombies`, `ts-kill-parent`, `ts-kill-self`. add your own
  with `make_script NAME 'body'`.
- `HOME=$WORK/home`.
- a 5 second `timeout`, so a hung pipeline fails instead of waiting
  forever.

`<<EOF` lets `$WORK` expand before the shell sees the text. `<<'EOF'`
(quoted) passes `$NAME` through untouched, which is what you want when
testing TinyShell's own expansion.

## writing good ones

check your expected output against bash. if bash disagrees with the test,
the test is probably the thing that's wrong.

keep each test to one idea, so that when it fails you know what broke.

and test the mistakes, not only the normal case. the most useful tests in
this suite are "a directory in PATH is skipped" and "SIGINT still kills the
child", and each of them exists because of one specific, common bug.
