/*
 * executor.c
 * Run an external program: fork, exec, wait.
 *
 * This is the entirety of "executing a command" on Unix, and it is worth
 * being able to write it out from memory:
 *
 *   fork()   the shell clones itself. The child gets an identical copy of
 *            every variable, every open file descriptor and the same address
 *            space. Nothing has happened yet. This is a fork bomb waiting
 *            for a purpose.
 *
 *   execvp() the child throws away its own program and is replaced, under the
 *            same pid, by the program named on the command line. This is the
 *            step where the shell "becomes" ls. Everything that survived the
 *            fork but was not an argument is now gone.
 *
 *   waitpid  only the parent waits, because only the parent still has a job
 *            afterwards. The child cannot afford to: it would be waiting on
 *            itself, forever.
 *
 * The split is the thing to remember. fork() is about *processes* and
 * concurrency; exec*() is about *programs* and what argv looks like. A
 * pipeline is several children between one fork/exec chain and the wait, and
 * a shell that blurs those two apart ends up with children that share
 * variables or parents that exec in the wrong order.
 */

#include "executor.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

int ts_launch(char **argv) {
  if (argv == NULL || argv[0] == NULL)
    return TS_STATUS_OK;

  pid_t pid = fork();
  if (pid == -1) {
    /* Out of process table. This is recoverable: the shell has no child to
     * reap, and the user gets a prompt again, so it is a message and not an
     * exit. */
    perror("tinyshell: fork");
    return TS_STATUS_NOT_FOUND;
  }

  if (pid == 0) {
    /* Child. The only thing that matters from here on is getting to execvp.
     *
     * If execvp fails we print the reason and _exit(). Not exit(): exit()
     * runs the atexit handlers, flushes the stdio buffers we inherited from
     * the shell a second time, and runs any cleanup this process was never
     * meant to do, all on its way out of a fork that already failed.
     * _exit() skips all of it and leaves immediately.
     *
     * (fprintf and strerror are not on the async-signal-safe list, which
     * matters in a threaded program where fork() may have landed in only one
     * of several threads. This shell is single-threaded, so it is fine.
     * Worth remembering before adding threads.) */
    execvp(argv[0], argv);

    /* Past this point execvp has failed and errno still describes why. Save
     * it before anything else can overwrite errno. The distinction the
     * status codes draw: we found the file but could not run it (126) versus
     * we never found it (127). */
    int err = errno;
    fprintf(stderr, "tinyshell: %s: %s\n", argv[0], strerror(err));
    _exit(err == EACCES || err == ENOEXEC || err == EISDIR
            ? TS_STATUS_NOT_EXECUTABLE
            : TS_STATUS_NOT_FOUND);
  }

  /* Parent. */
  int status = 0;
  while (waitpid(pid, &status, 0) == -1) {
    if (errno == EINTR) {
      /* A signal arrived while we were blocked in waitpid, Ctrl-C being the
       * obvious one. The child is still ours to reap, so go back and keep
       * waiting instead of reporting an error that did not happen. */
      continue;
    }
    perror("tinyshell: waitpid");
    return TS_STATUS_NOT_FOUND;
  }

  /* waitpid() packs the outcome into one int. Unpack it. */
  if (WIFEXITED(status))
    return WEXITSTATUS(status); /* the number the program chose to exit with */

  if (WIFSIGNALED(status)) {
    /* Killed by a signal. There is no exit code, so shells invent one:
     * 128 + the signal number, which is why a Ctrl-C'd command reports 130.
     * SIGINT is 2, and signal numbers are what makes this addition correct
     * rather than arbitrary. */
    return 128 + WTERMSIG(status);
  }

  return TS_STATUS_OK;
}