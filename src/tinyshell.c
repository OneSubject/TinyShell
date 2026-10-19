/*
 * tinyshell.c
 * Main entry point. REPL loop.
 */

#include <stdio.h>
#include <string.h>

#include "builtins.h"
#include "executor.h"
#include "parser.h"

/* One pass of the read-eval-print loop.
 * Returns TS_STOP when the shell should exit.
 */
static int shell_step(void) {
  char line[TS_MAX_LINE];
  char *argv[TS_MAX_ARGS];

  fputs(TS_PROMPT, stdout);
  fflush(stdout);

  if (fgets(line, sizeof(line), stdin) == NULL) {
    /* EOF (Ctrl-D) */
    putchar('\n');
    return TS_STOP;
  }

  if (ts_is_blank(line))
    return TS_CONTINUE;

  int argc = ts_tokenize(line, argv, TS_MAX_ARGS);
  if (argc <= 0)
    return TS_CONTINUE;

  if (ts_builtin_exec(argv) == TS_STOP)
    return TS_STOP;

  ts_launch(argv);
  return TS_CONTINUE;
}

int main(void) {
  while (shell_step() == TS_CONTINUE)
    ;
  return 0;
}
