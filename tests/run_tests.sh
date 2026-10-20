#!/bin/sh
#
# run_tests.sh: end-to-end tests for tinyshell.
#
# usage: sh tests/run_tests.sh path/to/tinyshell [milestone]
#        (normally via `make test` or `make test MILESTONE=5`)
#
# Every test feeds some lines to the shell on stdin, exactly as if they had
# been typed, and then checks what came out: stdout, stderr, exit status.
# That's the shell's real interface, so that's what gets tested.
#
# Each test belongs to a milestone. Tests for milestones up to the one you
# are on must pass. Tests for later milestones still run, but a failure is
# reported as TODO rather than FAIL, so the list doubles as a preview of
# what is coming. If a TODO test passes early, it says so.
#
# Adding a test: write a function that starts with `milestone N "what it
# checks"`, calls run_shell with the input on stdin, then calls expect_*.
# Then add its name to the list at the bottom.

set -u

TS_BIN=${1:?usage: $0 path/to/tinyshell [milestone]}
MILESTONE=${2:-3}

# Make the binary path absolute: the tests run from a scratch directory.
case $TS_BIN in
/*) ;;
*) TS_BIN=$(pwd)/$TS_BIN ;;
esac

if [ ! -x "$TS_BIN" ]; then
  echo "run_tests.sh: $TS_BIN is not an executable; run make build first" >&2
  exit 2
fi

# Everything a test creates goes in here, and it is removed afterwards.
WORK=$(mktemp -d "${TMPDIR:-/tmp}/tinyshell-tests.XXXXXX") || exit 2
trap 'rm -rf "$WORK"' EXIT
trap 'exit 130' INT TERM
WORK_REAL=$(cd "$WORK" && pwd -P)
cd "$WORK" || exit 2

# A shell that forgets to close a pipe end hangs forever (MILESTONE 5). A
# time limit turns that hang into a failure with a hint, instead of a test
# run that never finishes.
TIMEOUT="timeout 5"

if [ -t 1 ]; then
  GREEN=$(printf '\033[32m') RED=$(printf '\033[31m')
  YELLOW=$(printf '\033[33m') DIM=$(printf '\033[2m') RESET=$(printf '\033[0m')
else
  GREEN="" RED="" YELLOW="" DIM="" RESET=""
fi

# helpers

# Small executables the tests run through tinyshell. They live in a bin/
# directory that every test puts first on PATH.
mkdir -p "$WORK/bin" "$WORK/home"

make_script() { # make_script NAME BODY
  printf '#!/bin/sh\n%s\n' "$2" >"$WORK/bin/$1"
  chmod +x "$WORK/bin/$1"
}

make_script ts-hello 'echo hello from PATH'
make_script ts-slow 'sleep 0.3; echo bg-done'
make_script ts-zombies 'ps -o stat= --ppid "$PPID" | grep -c Z'
make_script ts-kill-parent 'kill -INT "$PPID"; sleep 0.1; echo child-finished'
make_script ts-kill-self 'kill -INT $$; echo survived-sigint'

TEST_PATH="$WORK/bin:/usr/local/bin:/usr/bin:/bin"

# run_shell [--no-home] [VAR=value ...] < input
#
# Runs tinyshell with stdin from the caller, a known PATH and HOME, and any
# extra environment given as arguments. --no-home runs it with HOME unset.
# Sets OUT, ERR and STATUS for the expect_* functions.
run_shell() {
  if [ "${1:-}" = "--no-home" ]; then
    shift
    env -u HOME PATH="$TEST_PATH" "$@" $TIMEOUT "$TS_BIN" \
      >"$WORK/stdout" 2>"$WORK/stderr"
  else
    env PATH="$TEST_PATH" HOME="$WORK/home" "$@" $TIMEOUT "$TS_BIN" \
      >"$WORK/stdout" 2>"$WORK/stderr"
  fi
  STATUS=$?
  OUT=$(cat "$WORK/stdout")
  ERR=$(cat "$WORK/stderr")
  if [ "$STATUS" -eq 124 ]; then
    fail "timed out after 5 seconds (a pipe end left open? a missing waitpid?)"
  fi
}

milestone() { # milestone N "description"
  TEST_MILESTONE=$1
  TEST_NAME=$2
}

fail() {
  PROBLEMS="$PROBLEMS
      $1"
}

# Indent a multi-line block so it reads clearly inside a failure report.
quote_block() {
  printf '%s\n' "$1" | sed 's/^/          | /'
}

expect_stdout() {
  if [ "$OUT" != "$1" ]; then
    fail "stdout was:
$(quote_block "$OUT")
      expected:
$(quote_block "$1")"
  fi
}

expect_stdout_contains() {
  case $OUT in
  *"$1"*) ;;
  *) fail "stdout did not contain '$1'. stdout was: $OUT" ;;
  esac
}

expect_stdout_lacks() {
  case $OUT in
  *"$1"*) fail "stdout should not contain '$1'. stdout was: $OUT" ;;
  esac
}

expect_stderr_contains() {
  case $ERR in
  *"$1"*) ;;
  *) fail "stderr did not contain '$1'. stderr was: ${ERR:-(empty)}" ;;
  esac
}

expect_status() {
  if [ "$STATUS" -ne "$1" ]; then
    fail "exit status was $STATUS, expected $1"
  fi
}

# MILESTONE 0: what the skeleton already does

test_runs_external_command() {
  milestone 0 "runs an external command with arguments"
  run_shell <<EOF
echo hello world
EOF
  expect_stdout "hello world"
  expect_status 0
}

test_blank_lines_are_ignored() {
  milestone 0 "blank lines do nothing"
  run_shell <<EOF


echo after-blank
EOF
  expect_stdout "after-blank"
}

test_eof_exits_with_last_status() {
  milestone 0 "end of input exits with the last command's status"
  run_shell <<EOF
false
EOF
  expect_status 1
}

test_syntax_error_is_reported() {
  milestone 0 "a syntax error is reported and the shell keeps going"
  run_shell <<EOF
| echo nope
echo still-here
EOF
  expect_stdout "still-here"
  expect_stderr_contains "syntax error"
}

test_signal_exit_status() {
  milestone 0 "a command killed by a signal reports 128 + N"
  run_shell <<EOF
ts-kill-self
EOF
  expect_status 130
}

# MILESTONE 1: cd

test_cd_changes_directory() {
  milestone 1 "cd changes the shell's own directory"
  run_shell <<EOF
cd $WORK/bin
pwd -P
EOF
  expect_stdout "$WORK_REAL/bin"
  expect_status 0
}

test_cd_without_argument_goes_home() {
  milestone 1 "cd with no argument goes to \$HOME"
  run_shell <<EOF
cd
pwd -P
EOF
  expect_stdout "$WORK_REAL/home"
}

test_cd_reports_errors() {
  milestone 1 "cd to a missing directory fails with errno's reason"
  run_shell <<EOF
cd $WORK/does-not-exist
echo still-here
cd $WORK/does-not-exist
EOF
  expect_stdout "still-here"
  expect_stderr_contains "No such file or directory"
  expect_status 1
}

test_cd_with_home_unset() {
  milestone 1 "cd with \$HOME unset fails cleanly"
  run_shell --no-home <<EOF
cd
EOF
  expect_stderr_contains "HOME not set"
  expect_status 1
}

# MILESTONE 2: exit

test_exit_with_status() {
  milestone 2 "exit N quits with status N"
  run_shell <<EOF
exit 7
echo not-reached
EOF
  expect_stdout ""
  expect_status 7
}

test_exit_uses_last_status() {
  milestone 2 "exit with no argument reuses the last status"
  run_shell <<EOF
false
exit
EOF
  expect_status 1
}

test_exit_rejects_non_numbers() {
  milestone 2 "exit abc is an error, not a quiet exit 0"
  run_shell <<EOF
exit abc
echo still-here
EOF
  expect_stdout "still-here"
  expect_stderr_contains "not a number"
}

test_exit_truncates_to_8_bits() {
  milestone 2 "exit 258 quits with 2 (only 8 bits survive)"
  run_shell <<EOF
exit 258
EOF
  expect_status 2
}

# MILESTONE 3: PATH lookup

test_path_lookup() {
  milestone 3 "a bare name is found by searching \$PATH"
  run_shell <<EOF
ts-hello
EOF
  expect_stdout "hello from PATH"
}

test_path_skips_directories() {
  milestone 3 "a directory with the command's name is skipped"
  mkdir -p "$WORK/decoy/ts-hello"
  run_shell PATH="$WORK/decoy:$TEST_PATH" <<EOF
ts-hello
EOF
  expect_stdout "hello from PATH"
}

test_absolute_path() {
  milestone 3 "a name with a slash is run directly"
  run_shell <<EOF
$WORK/bin/ts-hello
EOF
  expect_stdout "hello from PATH"
}

test_command_not_found() {
  milestone 3 "an unknown command reports 127"
  run_shell <<EOF
ts-no-such-command
EOF
  expect_stderr_contains "command not found"
  expect_status 127
}

test_not_executable() {
  milestone 3 "a file without the x bit reports 126"
  printf 'echo hi\n' >"$WORK/bin/ts-noexec"
  chmod -x "$WORK/bin/ts-noexec"
  run_shell <<EOF
ts-noexec
EOF
  expect_stderr_contains "Permission denied"
  expect_status 126
}

# MILESTONE 4: redirections

test_redirect_stdout() {
  milestone 4 "> sends stdout to a file, and only for that command"
  rm -f "$WORK/out"
  run_shell <<EOF
echo into-file > $WORK/out
echo on-screen
cat $WORK/out
EOF
  expect_stdout "on-screen
into-file"
}

test_redirect_append() {
  milestone 4 ">> appends instead of truncating"
  rm -f "$WORK/log"
  run_shell <<EOF
echo one > $WORK/log
echo two >> $WORK/log
cat $WORK/log
EOF
  expect_stdout "one
two"
}

test_redirect_stdin() {
  milestone 4 "< reads stdin from a file"
  printf 'shout\n' >"$WORK/in"
  run_shell <<EOF
tr a-z A-Z < $WORK/in
EOF
  expect_stdout "SHOUT"
}

test_redirect_stderr() {
  milestone 4 "2> sends stderr to a file"
  rm -f "$WORK/err"
  run_shell <<EOF
ls $WORK/does-not-exist 2> $WORK/err
EOF
  expect_stdout ""
  if [ ! -s "$WORK/err" ]; then
    fail "$WORK/err should hold ls's error message"
  fi
}

test_redirect_missing_input() {
  milestone 4 "< from a missing file fails without running the command"
  run_shell <<EOF
echo should-not-run < $WORK/does-not-exist
EOF
  expect_stdout ""
  expect_stderr_contains "No such file or directory"
  expect_status 1
}

# MILESTONE 5: pipes

test_pipe_two_commands() {
  milestone 5 "a | b connects a's stdout to b's stdin"
  run_shell <<EOF
echo hello | tr a-z A-Z
EOF
  expect_stdout "HELLO"
}

test_pipe_three_commands() {
  milestone 5 "a | b | c works for any number of commands"
  run_shell <<EOF
seq 5 | sort -r | head -n 1
EOF
  expect_stdout "5"
}

test_pipe_status_is_last_command() {
  milestone 5 "a pipeline's status is its last command's"
  run_shell <<EOF
true | false
EOF
  expect_status 1
}

# MILESTONE 6: background jobs

test_background_does_not_block() {
  milestone 6 "cmd & returns to the prompt immediately"
  run_shell <<EOF
ts-slow &
echo fg-first
sleep 0.6
echo end
EOF
  expect_stdout "fg-first
bg-done
end"
  expect_stderr_contains "[1]"
}

test_background_jobs_are_reaped() {
  milestone 6 "a finished background job does not stay a zombie"
  run_shell <<EOF
ts-slow &
sleep 0.6
ts-zombies
EOF
  expect_stdout "bg-done
0"
}

# MILESTONE 7: signals

test_sigint_does_not_kill_shell() {
  milestone 7 "SIGINT does not kill the shell"
  run_shell <<EOF
ts-kill-parent
echo shell-alive
EOF
  expect_stdout "child-finished
shell-alive"
}

test_sigint_still_kills_child() {
  milestone 7 "SIGINT still kills the foreground program"
  run_shell <<EOF
ts-kill-self
EOF
  expect_stdout_lacks "survived-sigint"
  expect_status 130
}

# MILESTONE 8: quoting

test_single_quotes() {
  milestone 8 "single quotes keep spaces and specials literal"
  run_shell <<'EOF'
echo 'a   b' '|' '>'
EOF
  expect_stdout "a   b | >"
}

test_double_quotes() {
  milestone 8 "double quotes make one argument"
  run_shell <<'EOF'
echo "x   y" z
EOF
  expect_stdout "x   y z"
}

test_backslash() {
  milestone 8 "backslash escapes the next character"
  run_shell <<'EOF'
echo a\ b \|
EOF
  expect_stdout "a b |"
}

test_empty_argument() {
  milestone 8 "'' is an empty argument, not nothing"
  run_shell <<'EOF'
printf [%s] ''
EOF
  expect_stdout "[]"
}

test_unterminated_quote() {
  milestone 8 "an unterminated quote is a syntax error"
  run_shell <<'EOF'
echo 'oops
EOF
  expect_stdout ""
  expect_stderr_contains "unterminated"
  expect_status 2
}

# MILESTONE 9: environment variables

test_export_and_expand() {
  milestone 9 "export sets a variable, \$NAME expands it"
  run_shell <<'EOF'
export TS_GREETING=hi
echo $TS_GREETING there
EOF
  expect_stdout "hi there"
}

test_export_reaches_children() {
  milestone 9 "exported variables are inherited by children"
  run_shell <<'EOF'
export TS_CHILD=inherited
printenv TS_CHILD
EOF
  expect_stdout "inherited"
}

test_unset() {
  milestone 9 "unset removes a variable"
  run_shell <<'EOF'
export TS_GONE=1
unset TS_GONE
echo [$TS_GONE]
EOF
  expect_stdout "[]"
}

test_last_status_variable() {
  milestone 9 "\$? holds the last exit status"
  run_shell <<'EOF'
false
echo $?
EOF
  expect_stdout "1"
}

test_single_quotes_block_expansion() {
  milestone 9 "'\$HOME' is not expanded, \"\$HOME\" is"
  run_shell <<'EOF'
echo '$HOME' "$HOME"
EOF
  expect_stdout "\$HOME $WORK/home"
}

# MILESTONE 10: history (stretch)

test_history() {
  milestone 10 "history lists earlier commands"
  run_shell <<'EOF'
echo one
echo two
history
EOF
  expect_stdout_contains "echo one"
  expect_stdout_contains "echo two"
}

# runner

passed=0
failed=0
todo=0

run_test() {
  PROBLEMS=""
  TEST_MILESTONE=0
  TEST_NAME=$1
  "$1"

  label=$(printf 'M%-2s %s' "$TEST_MILESTONE" "$TEST_NAME")

  if [ -z "$PROBLEMS" ]; then
    passed=$((passed + 1))
    if [ "$TEST_MILESTONE" -gt "$MILESTONE" ]; then
      printf '  %sPASS%s  %s %s(ahead of MILESTONE=%s)%s\n' \
        "$GREEN" "$RESET" "$label" "$DIM" "$MILESTONE" "$RESET"
    else
      printf '  %sPASS%s  %s\n' "$GREEN" "$RESET" "$label"
    fi
  elif [ "$TEST_MILESTONE" -gt "$MILESTONE" ]; then
    todo=$((todo + 1))
    printf '  %sTODO%s  %s\n' "$YELLOW" "$RESET" "$label"
  else
    failed=$((failed + 1))
    printf '  %sFAIL%s  %s%s\n' "$RED" "$RESET" "$label" "$PROBLEMS"
  fi
}

for test_name in \
  test_runs_external_command \
  test_blank_lines_are_ignored \
  test_eof_exits_with_last_status \
  test_syntax_error_is_reported \
  test_signal_exit_status \
  test_cd_changes_directory \
  test_cd_without_argument_goes_home \
  test_cd_reports_errors \
  test_cd_with_home_unset \
  test_exit_with_status \
  test_exit_uses_last_status \
  test_exit_rejects_non_numbers \
  test_exit_truncates_to_8_bits \
  test_path_lookup \
  test_path_skips_directories \
  test_absolute_path \
  test_command_not_found \
  test_not_executable \
  test_redirect_stdout \
  test_redirect_append \
  test_redirect_stdin \
  test_redirect_stderr \
  test_redirect_missing_input \
  test_pipe_two_commands \
  test_pipe_three_commands \
  test_pipe_status_is_last_command \
  test_background_does_not_block \
  test_background_jobs_are_reaped \
  test_sigint_does_not_kill_shell \
  test_sigint_still_kills_child \
  test_single_quotes \
  test_double_quotes \
  test_backslash \
  test_empty_argument \
  test_unterminated_quote \
  test_export_and_expand \
  test_export_reaches_children \
  test_unset \
  test_last_status_variable \
  test_single_quotes_block_expansion \
  test_history; do
  run_test "$test_name"
done

echo
printf '%s passed, %s failed, %s todo  (MILESTONE=%s)\n' \
  "$passed" "$failed" "$todo" "$MILESTONE"

if [ "$todo" -gt 0 ]; then
  echo "next: finish the lowest TODO milestone, then run make test MILESTONE=<it>"
fi

[ "$failed" -eq 0 ]
