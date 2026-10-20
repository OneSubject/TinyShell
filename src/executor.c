/*
 * executor.c
 * Run an external program: fork, exec, wait.
 *
 * This is the entirety of "executing a command" on Unix, and it is worth
 * being able to write it out from memory:
 *
 *   fork()   the shell clones itself. The child gets an identical copy of
 *            every variable, every open file descriptor and the same address
 *            space. Nothing has happened yet.
 *
 *   (setup)  the child, and only the child, adjusts its own world: signal
 *            dispositions (MILESTONE 7), file descriptors for redirection
 *            (MILESTONE 4) and pipes (MILESTONE 5). The shell is unaffected,
 *            because the child's copies are its own.
 *
 *   execv()  the child throws away its own program and is replaced, under the
 *            same pid, by the program named on the command line. This is the
 *            step where the shell "becomes" ls. Everything that survived the
 *            fork but was not an argument, an fd or the environment is now
 *            gone.
 *
 *   waitpid  only the parent waits, because only the parent still has a job
 *            afterwards. Until it does, the finished child is a zombie.
 *
 * fork() deals with processes and exec with programs. Most of the interesting
 * shell features happen in the child between those two calls.
 */

#include "executor.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "error.h"
#include "path.h"
#include "redirect.h"
#include "signals.h"

/* The child's half of fork/exec. Never returns: either exec replaces this
 * process, or the child exits with 126/127.
 *
 * _Noreturn (C11) tells the compiler, and the reader, that control never
 * comes back. It also turns "forgot the _exit() at the end" into a warning.
 *
 * Why _exit() and not exit()? exit() runs atexit handlers and flushes stdio
 * buffers, and the buffers in this child are copies of the shell's. Flushing
 * them here would print the shell's pending output a second time. _exit()
 * skips all of that and leaves immediately.
 *
 * (fprintf and strerror are not on the async-signal-safe list, which matters
 * in a threaded program where fork() may have landed in only one of several
 * threads. This shell is single-threaded, so it is fine. Worth remembering
 * before adding threads.) */
static _Noreturn void exec_child(const ts_command *command) {
  /* MILESTONE 7: undo the shell's own signal handling, so Ctrl-C can kill
   * this program even though it no longer kills the shell. */
  ts_signals_reset_for_child();

  /* MILESTONE 4: point fds 0, 1 and 2 where the redirects say. */
  if (ts_redirect_apply(command) == -1)
    _exit(TS_STATUS_FAILURE);

  /* MILESTONE 3: turn "ls" into "/usr/bin/ls". */
  char path[TS_MAX_PATH];
  if (ts_find_in_path(command->argv[0], path, sizeof(path)) == -1) {
    if (errno == ENOENT && strchr(command->argv[0], '/') == NULL) {
      ts_errorx("%s: command not found", command->argv[0]);
      _exit(TS_STATUS_NOT_FOUND);
    }
    ts_error("%s", command->argv[0]);
    _exit(errno == EACCES ? TS_STATUS_NOT_EXECUTABLE : TS_STATUS_NOT_FOUND);
  }

  execv(path, command->argv);

  /* Past this point execv has failed and errno still describes why. Save it
   * before anything else can overwrite it. The distinction the status codes
   * draw: we found the file but could not run it (126) versus it was not
   * there at all (127). */
  int saved_errno = errno;
  ts_error("%s", command->argv[0]);
  _exit(saved_errno == EACCES || saved_errno == ENOEXEC ||
            saved_errno == EISDIR
          ? TS_STATUS_NOT_EXECUTABLE
          : TS_STATUS_NOT_FOUND);
}

/* Fork a child that runs `command`. Returns the child's pid to the parent,
 * or -1 (with a message printed) if fork failed. */
static pid_t spawn(const ts_command *command) {
  /* Flush before forking. Anything still sitting in the shell's stdout
   * buffer would be copied into the child; it would never be printed there
   * (exec discards it), but it would come out of the shell *after* the
   * child's output, in the wrong order. fflush(NULL) flushes every stream. */
  fflush(NULL);

  pid_t pid = fork();
  if (pid == -1) {
    /* Out of processes or memory. This is recoverable: the user gets a
     * prompt again, so it is a message and not an exit. */
    ts_error("fork");
    return -1;
  }

  if (pid == 0)
    exec_child(command); /* never returns */

  return pid;
}

/* Wait for one child and translate its fate into a shell exit status. */
static int wait_for_child(pid_t pid) {
  int status = 0;

  while (waitpid(pid, &status, 0) == -1) {
    if (errno == EINTR) {
      /* A signal arrived while we were blocked in waitpid, Ctrl-C being the
       * obvious one once MILESTONE 7 stops it from killing the shell. The
       * child is still ours to reap, so go back and keep waiting instead of
       * reporting an error that did not happen. */
      continue;
    }
    ts_error("waitpid");
    return TS_STATUS_FAILURE;
  }

  /* waitpid() packs the outcome into one int. Unpack it with the macros;
   * the bit layout is not something to rely on. */
  if (WIFEXITED(status))
    return WEXITSTATUS(status); /* the number the program chose to exit with */

  if (WIFSIGNALED(status)) {
    /* Killed by a signal. There is no exit code, so shells invent one:
     * 128 + the signal number, which is why a Ctrl-C'd command reports 130.
     * SIGINT is 2. */
    return TS_STATUS_SIGNAL_BASE + WTERMSIG(status);
  }

  return TS_STATUS_OK;
}

static int run_foreground(const ts_command *command) {
  pid_t pid = spawn(command);
  if (pid == -1)
    return TS_STATUS_FAILURE;
  return wait_for_child(pid);
}

/* MILESTONE 5: pipelines.
 *
 * `a | b | c` is three children, each with its stdout connected to the
 * next one's stdin:
 *
 *        pipe 0              pipe 1
 *   a  [w]-----[r]  b  [w]-----[r]  c
 *   fd1          fd0  fd1         fd0
 *
 * For N commands you need N - 1 pipes. A plan that works:
 *
 * TODO(M5): keep `int read_end = -1`: the read end of the previous pipe,
 *           which becomes the next command's stdin.
 *
 * TODO(M5): for each command i:
 *             - unless it is the last one, pipe(fds) for its stdout.
 *             - fork(). In the child:
 *                 if read_end != -1: dup2(read_end, STDIN_FILENO)
 *                 if not last:       dup2(fds[1], STDOUT_FILENO)
 *                 close every pipe fd this process still holds (read_end,
 *                 fds[0], fds[1]), then exec_child(command).
 *               Redirects inside exec_child() run after the dup2s, so
 *               `a > f | b` sends a's output to f, as in bash.
 *             - in the parent: close read_end (if any) and fds[1]; the
 *               children have their copies. Then read_end = fds[0].
 *
 * TODO(M5): wait for every child (keep their pids in an array of
 *           TS_MAX_PIPELINE). The pipeline's status is the last one's.
 *
 * The bug everyone hits once: a reader only sees EOF when *every* copy of
 * the write end is closed, including the parent's and its siblings'. Miss
 * one close() and `ls | wc` hangs forever, because wc is still waiting for
 * a writer that will never write. (The tests run under `timeout` for this
 * reason.) If a pipe() or fork() fails halfway, close what you opened and
 * wait for the children already started.
 *
 * Builtins in a pipeline are run like any other command, which means
 * through PATH, where there is no `cd`. bash runs them in a subshell;
 * that is an extension.
 *
 * Test with: make test MILESTONE=5
 */
static int run_pipeline(const ts_pipeline *pipeline) {
  (void)pipeline;
  ts_errorx("pipes ('|') are not implemented yet (MILESTONE 5)");
  return TS_STATUS_USAGE;
}

/* MILESTONE 6: background jobs.
 *
 * TODO(M6): spawn() the command (start with single commands; a background
 *           *pipeline* can come later), and do NOT wait for it.
 *
 * TODO(M6): #include "jobs.h", then ts_jobs_add(pid, command_line) and
 *           print "[job] pid" to stderr, the way bash does. Return
 *           TS_STATUS_OK: starting the job succeeded, and its real exit
 *           status is not known yet.
 *
 * TODO(M6): fill in ts_jobs_reap() in jobs.c, so the finished child does
 *           not stay a zombie.
 *
 * A background job still shares the terminal. If it reads from stdin it
 * competes with the shell for your keystrokes; real shells stop it with
 * SIGTTIN (docs/src/concepts/job-control.md). Out of scope here.
 *
 * Test with: make test MILESTONE=6
 */
static int run_background(const ts_pipeline *pipeline,
                          const char *command_line) {
  (void)pipeline;
  (void)command_line;
  ts_errorx("background jobs ('&') are not implemented yet (MILESTONE 6)");
  return TS_STATUS_USAGE;
}

int ts_execute(const ts_pipeline *pipeline, const char *command_line) {
  if (pipeline == NULL || pipeline->count == 0)
    return TS_STATUS_OK;

  if (pipeline->background)
    return run_background(pipeline, command_line);

  if (pipeline->count > 1)
    return run_pipeline(pipeline);

  return run_foreground(&pipeline->commands[0]);
}
