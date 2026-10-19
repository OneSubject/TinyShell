/*
 * executor.c
 * Launch an external program: fork, execvp, waitpid.
 *
 * Foreground only for now. No pipes, no redirection, no background jobs.
 */

#include "executor.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

/* Run argv[0] with args argv. Waits for the child to finish.
 * Returns TS_CONTINUE.
 */
int ts_launch(char **argv) {
  if (argv == NULL || argv[0] == NULL)
    return TS_CONTINUE;

  pid_t pid = fork();
  if (pid == -1) {
    perror("tinyshell: fork");
    return TS_CONTINUE;
  }

  if (pid == 0) {
    /* child */
    if (execvp(argv[0], argv) == -1)
      perror("tinyshell");
    _exit(EXIT_FAILURE);
  }

  /* parent: wait for child */
  int status = 0;
  do {
    if (waitpid(pid, &status, WUNTRACED) == -1) {
      perror("tinyshell: waitpid");
      break;
    }
  } while (!WIFEXITED(status) && !WIFSIGNALED(status));

  return TS_CONTINUE;
}
