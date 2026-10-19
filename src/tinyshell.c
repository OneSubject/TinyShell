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
 *     read a line -> tokenize it -> ask the builtins -> fork/exec if nobody
 *     answered -> remember the exit status
 *
 * Four files, one step each. Step 4 is the one that sounds obvious and is
 * the easiest to leave out: a status that only lives inside shell_step() is
 * already gone by the time the next line arrives.
 */

#include <stdio.h>
#include <string.h>

#include "builtins.h"
#include "executor.h"
#include "parser.h"
#include "tinyshell.h"

/* Exit status of the most recent command.
 *
 * This is what $? expands to, and $? is not implemented yet (README, step
 * 7). It is kept now anyway, for two reasons: a status that has to be
 * reconstructed later cannot be, and `exit` with no argument already needs
 * the previous command's status to quit the right way.
 */
static int last_status = TS_STATUS_OK;

/* Outcome of reading one line.
 *
 * A bool would not be enough, and finding out why is the point of the enum:
 * EOF ends the shell, but an over-long line is just an error the user gets
 * told about. Collapsing both into "stop reading" is how a shell eats the
 * rest of somebody's input script.
 */
typedef enum {
  LINE_OK,       /* a full line, newline included, is in the buffer  */
  LINE_EOF,      /* Ctrl-D, or stdin reached end of file             */
  LINE_TOO_LONG, /* over TS_MAX_LINE; the rest was drained and lost */
} line_result;

/* Read one line into `line`.
 *
 * fgets() stops at the newline *or* when the buffer fills, whichever comes
 * first, and it never says which happened. Guess wrong and a long command
 * silently becomes two commands, the second one built from a fragment the
 * user never typed. So check for a missing newline, drain whatever is left of
 * the line, and tell the caller which of the three things happened.
 */
static line_result read_line(char *line, size_t size) {
  if (fgets(line, (int)size, stdin) == NULL)
    return LINE_EOF;

  size_t len = strlen(line);
  if (len == size - 1 && line[len - 1] != '\n') {
    int c;
    while ((c = getchar()) != '\n' && c != EOF)
      ;

    fprintf(stderr, "tinyshell: line too long, ignored\n");
    return LINE_TOO_LONG;
  }

  return LINE_OK;
}

/* Write the prompt.
 *
 * To stderr, not stdout. The prompt is the shell talking to the person, not
 * output from the program they asked for, so it must not land in a pipeline or
 * a redirect. `tinyshell > out.txt` should capture ls's output and nothing
 * else. bash does the same, and the reason is worth being able to give:
 * stdout belongs to the command, stderr to the shell.
 *
 * stderr is unbuffered by default so the prompt appears before the read
 * rather than whenever the buffer happens to flush.
 */
static void prompt(void) {
  fputs(TS_PROMPT, stderr);
}

/* One turn of the read-eval loop.
 *
 * Returns TS_CONTINUE to prompt again, TS_STOP when the shell is done.
 */
static int shell_step(void) {
  char line[TS_MAX_LINE];
  char *argv[TS_MAX_ARGS];

  prompt();

  switch (read_line(line, sizeof(line))) {
  case LINE_EOF:
    fputc('\n', stderr); /* step off the prompt line before vanishing */
    return TS_STOP;
  case LINE_TOO_LONG:
    return TS_CONTINUE; /* the line was consumed; the next one is fine */
  case LINE_OK:
    break;
  }

  if (ts_is_blank(line))
    return TS_CONTINUE;

  int argc = ts_tokenize(line, argv, TS_MAX_ARGS);
  if (argc == 0)
    return TS_CONTINUE;

  /* Start from the previous command's status. A builtin that succeeds leaves
   * it alone, which is what makes `exit` with no argument mean "quit the way
   * the last thing finished", the same rule every other shell follows.
   * Builtins that can fail will overwrite it when they get here. */
  int status = last_status;

  switch (ts_builtin_exec(argv, &status)) {
  case TS_NOT_A_BUILTIN:
    /* Nobody in the shell's own table claimed this name, so let the OS
       * look for it on PATH. This is the fork. */
    status = ts_launch(argv);
    break;
  case TS_BUILTIN_EXIT:
    last_status = status;
    return TS_STOP;
  case TS_BUILTIN_OK:
    break;
  }

  last_status = status;
  return TS_CONTINUE;
}

int main(void) {
  while (shell_step() == TS_CONTINUE)
    ;

  /* The shell exits with the status of the last command it ran, which is how
   * `tinyshell <<EOF ... EOF` is useful from a script at all. */
  return last_status;
}
