/*
 * jobs.c
 * MILESTONE 6: the job table.
 *
 * Every background child has to be waited for eventually. When it exits, the
 * kernel holds on to its exit status until the parent collects it with
 * waitpid(). Until then the child is a *zombie*: it has finished, uses no
 * memory or CPU, but still occupies a slot in the process table. A shell that
 * starts background jobs and never waits for them leaks those slots one by
 * one until fork() starts failing.
 *
 * The table is a fixed array, because the number of jobs a person runs at
 * once is small and a fixed array cannot leak.
 */

#include "jobs.h"

#include <stdbool.h>
#include <stdio.h>
#include <sys/types.h>

#include "error.h"
#include "signals.h"

/* Room for a readable "[1] Done  sleep 10" line. Longer commands are
 * shortened for display only; the job itself is not affected. */
#define TS_MAX_JOB_COMMAND 128

typedef struct {
  bool in_use;
  int id;    /* what the user sees: [1], [2], ...       */
  pid_t pid; /* what the kernel knows it by               */
  char command[TS_MAX_JOB_COMMAND];
} ts_job;

static ts_job jobs[TS_MAX_JOBS];

int ts_jobs_add(pid_t pid, const char *command_line) {
  /* Job numbers are reused from the lowest free one up, like bash: when
   * job 1 finishes, the next one is [1] again. */
  for (int slot = 0; slot < TS_MAX_JOBS; slot++) {
    if (jobs[slot].in_use)
      continue;

    jobs[slot].in_use = true;
    jobs[slot].id = slot + 1;
    jobs[slot].pid = pid;
    /* snprintf() always NUL-terminates and never overflows, which is why
     * it is used here instead of strncpy(): strncpy() leaves the result
     * unterminated when the source is too long. */
    snprintf(jobs[slot].command, sizeof(jobs[slot].command), "%s",
             command_line);
    return jobs[slot].id;
  }

  ts_errorx("too many background jobs (limit %d)", TS_MAX_JOBS);
  return -1;
}

/* MILESTONE 6: reaping.
 *
 * TODO(M6): add <errno.h> and <sys/wait.h> to the includes.
 *
 * TODO(M6): for each job in use, call waitpid(job->pid, &status, WNOHANG).
 *             returns 0         still running; leave it alone
 *             returns job->pid  finished: print "[id] Done  command" to
 *                               stderr (or "[id] Exit N" / "Killed by
 *                               signal N" if you want to decode status
 *                               with WIFEXITED/WEXITSTATUS) and mark the
 *                               slot free
 *             returns -1        errno ECHILD means someone else already
 *                               reaped it; free the slot. EINTR: try again.
 *
 *           Waiting on each job's own pid, rather than waitpid(-1, ...),
 *           guarantees this never collects a child it did not start in the
 *           background. At the top of the loop there is no foreground
 *           child, so waitpid(-1, &status, WNOHANG) in a loop would also
 *           work here; it is in a SIGCHLD handler that -1 becomes a bug
 *           (see signals.c).
 *
 * The early return on ts_signals_take_sigchld() is an optimisation that
 * becomes real in MILESTONE 7; until then it always says "check".
 *
 * Test with: make test MILESTONE=6
 */
void ts_jobs_reap(void) {
  if (!ts_signals_take_sigchld())
    return;

  /* TODO(M6): the loop described above goes here. */
}
