/*
 * tinyshell.c
 * The REPL: read a line, run it, prompt again.
 *
 * A shell is a loop that never ends, and nearly every design decision here
 * follows from that. The shell must be ready to read the next line at any
 * moment: after a background job, after a signal, after a syntax error, after
 * a command that does not exist. Failure is never fatal. The only thing that
 * ends the loop is the user asking for it.
 *
 * The whole program is that loop:
 *
 *     reap finished jobs -> read a line -> lex -> parse -> builtin, or
 *     fork/exec if no builtin answered -> remember the exit status
 *
 * The last step is the one that sounds obvious and is the easiest to leave
 * out: a status that only lives inside shell_step() is already gone by the
 * time the next line arrives.
 */

#include "tinyshell.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "builtins.h"
#include "error.h"
#include "executor.h"
#include "expand.h"
#include "history.h"
#include "jobs.h"
#include "parser.h"
#include "signals.h"

/* Exit status of the most recent command: what $? expands to (MILESTONE 9),
 * and what `exit` with no argument quits with. */
static int last_status = TS_STATUS_OK;

/* Whether a person is typing at us. Decided once, at startup. */
static bool interactive = false;

/* Outcome of reading one line.
 *
 * A bool isn't enough here. EOF ends the shell, but an over-long line is just
 * an error the user gets told about. Collapsing both into "stop reading" is
 * how a shell eats the rest of somebody's input script.
 */
typedef enum {
  LINE_OK,          /* a full line is in the buffer                     */
  LINE_EOF,         /* Ctrl-D, end of a script, or an unrecoverable error */
  LINE_TOO_LONG,    /* over TS_MAX_LINE; the rest was drained and lost  */
  LINE_INTERRUPTED, /* a signal (Ctrl-C, MILESTONE 7) cut the read short */
} line_result;

/* Read one line into `line`.
 *
 * fgets() stops at the newline *or* when the buffer fills, whichever comes
 * first, and it never says which happened. Guess wrong and a long command
 * silently becomes two commands, the second one built from a fragment the
 * user never typed. So check for a missing newline, drain whatever is left of
 * the line, and tell the caller which of the things happened.
 *
 * fgets() also returns NULL for two different reasons: end of input, and an
 * error. Once the shell catches SIGINT, "error" includes EINTR, which just
 * means Ctrl-C was pressed at the prompt. Treating that as EOF would make
 * Ctrl-C quit the shell, the exact thing MILESTONE 7 is meant to prevent.
 */
static line_result read_line(char *line, size_t size) {
  if (fgets(line, (int)size, stdin) == NULL) {
    if (ferror(stdin)) {
      if (errno == EINTR) {
        clearerr(stdin); /* otherwise every later fgets() fails at once */
        return LINE_INTERRUPTED;
      }
      ts_error("read");
    }
    return LINE_EOF;
  }

  size_t length = strlen(line);
  if (length == size - 1 && line[length - 1] != '\n') {
    int c;
    while ((c = getchar()) != '\n' && c != EOF)
      ;

    ts_errorx("line too long (limit %d bytes), ignored", TS_MAX_LINE - 1);
    return LINE_TOO_LONG;
  }

  /* The newline is not part of the command. Removing it here means nothing
   * later has to wonder whether the last token ends in '\n'. */
  line[strcspn(line, "\n")] = '\0';
  return LINE_OK;
}

/* Write the prompt.
 *
 * To stderr, not stdout. The prompt is the shell talking to the person, not
 * output from the program they asked for, so it must not land in a pipeline or
 * a redirect. `tinyshell > out.txt` should capture ls's output and nothing
 * else. bash does the same: stdout belongs to the command, stderr to the
 * shell.
 *
 * Only when stdin is a terminal. When the input is a file or a pipe, as in
 * `tinyshell < script` or the test suite, nobody is there to read a prompt,
 * and it would only clutter the output. bash makes the same decision.
 *
 * stderr is unbuffered by default so the prompt appears before the read
 * rather than whenever the buffer happens to flush.
 */
static void prompt(void) {
  if (interactive)
    fputs(TS_PROMPT, stderr);
}

/* Tell the user why their line was rejected by the lexer. */
static void report_lex_error(ts_lex_result result) {
  switch (result) {
  case TS_LEX_OK:
    break;
  case TS_LEX_TOO_MANY_TOKENS:
    ts_errorx("too many words on one line (limit %d)", TS_MAX_ARGS - 1);
    break;
  case TS_LEX_TOO_LONG:
    ts_errorx("line too long after expansion");
    break;
  case TS_LEX_UNTERMINATED_QUOTE:
    ts_errorx("syntax error: unterminated quote");
    break;
  }
}

/* Run a builtin in the shell's own process. Returns TS_STOP for `exit`. */
static int run_builtin(ts_command *command, int *status) {
  /* Redirecting a builtin would rewire the shell's *own* fds, which is
   * a different problem from redirecting a child (see the stretch note in
   * redirect.c). Refuse rather than silently ignore the redirect. */
  if (command->redirect_count > 0) {
    ts_errorx("%s: redirecting a builtin is not supported",
              command->argv[0]);
    *status = TS_STATUS_USAGE;
    return TS_CONTINUE;
  }

  if (ts_builtin_exec(command->argv, status) == TS_BUILTIN_EXIT)
    return TS_STOP;
  return TS_CONTINUE;
}

/* One turn of the read-eval loop.
 *
 * Returns TS_CONTINUE to prompt again, TS_STOP when the shell is done.
 *
 * `tokens` and `pipeline` together are about 30 KB, comfortably within the
 * default 8 MB stack. They are locals on purpose: they are rebuilt from
 * scratch for every line, so nothing from one command can leak into the
 * next, and there is nothing to free.
 */
static int shell_step(void) {
  char line[TS_MAX_LINE];
  ts_token_list tokens;
  ts_pipeline pipeline;

  /* MILESTONE 6: collect background jobs that finished while the previous
   * command ran, before the prompt, so "[1] Done" appears in a sensible
   * place. */
  ts_jobs_reap();

  prompt();

  switch (read_line(line, sizeof(line))) {
  case LINE_EOF:
    if (interactive)
      fputc('\n', stderr); /* step off the prompt line before vanishing */
    return TS_STOP;
  case LINE_INTERRUPTED:
    if (interactive)
      fputc('\n', stderr); /* Ctrl-C at the prompt: start a fresh line */
    return TS_CONTINUE;
  case LINE_TOO_LONG:
    last_status = TS_STATUS_USAGE;
    return TS_CONTINUE; /* the line was consumed; the next one is fine */
  case LINE_OK:
    break;
  }

  if (ts_is_blank(line))
    return TS_CONTINUE;

  ts_history_add(line); /* MILESTONE 10 */

  ts_lex_result lexed = ts_tokenize(line, &tokens);
  if (lexed != TS_LEX_OK) {
    report_lex_error(lexed);
    last_status = TS_STATUS_USAGE;
    return TS_CONTINUE;
  }
  if (tokens.count == 0)
    return TS_CONTINUE;

  if (!ts_parse(&tokens, &pipeline)) {
    last_status = TS_STATUS_USAGE;
    return TS_CONTINUE;
  }

  /* Start from the previous command's status, which is what `exit` with no
   * argument needs to see. Everything below overwrites it. */
  int status = last_status;
  int next = TS_CONTINUE;
  ts_command *first = &pipeline.commands[0];

  /* Builtins are consulted before any fork, never after: once a child
   * exists, a change it makes cannot come back to the shell. Only a lone,
   * foreground command can be a builtin; inside a pipeline or with `&`
   * there is a child involved by definition. */
  if (pipeline.count == 1 && !pipeline.background &&
      ts_is_builtin(first->argv[0]))
    next = run_builtin(first, &status);
  else
    status = ts_execute(&pipeline, line);

  last_status = status;
  ts_expand_set_last_status(status); /* for $?, MILESTONE 9 */
  return next;
}

int main(void) {
  interactive = isatty(STDIN_FILENO);

  ts_signals_install(); /* MILESTONE 7 */

  while (shell_step() == TS_CONTINUE)
    ;

  /* The shell exits with the status of the last command it ran, which is how
   * `tinyshell <<EOF ... EOF` is useful from a script at all. */
  return last_status;
}
