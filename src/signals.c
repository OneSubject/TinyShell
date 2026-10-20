/*
 * signals.c
 * MILESTONE 7: signal handling.
 *
 * When you press Ctrl-C, the terminal sends SIGINT to every process in the
 * foreground process group, and right now that includes the shell. The
 * default action for SIGINT is "terminate", so the shell dies along with
 * whatever it was running.
 *
 * The fix has two halves, and forgetting the second one is the classic bug:
 *
 *   1. the shell catches SIGINT, so it survives;
 *   2. every child puts SIGINT back to the default before exec.
 *
 * Why is (2) needed? Because of what exec does with signal dispositions:
 * caught signals are reset to default (the handler's code is gone, so
 * there is nothing left to call), but *ignored* signals stay ignored. A
 * shell that uses SIG_IGN and forgets step 2 produces children that cannot
 * be Ctrl-C'd at all. The test "foreground child still dies from SIGINT"
 * is there to catch that.
 */

#include "signals.h"

/* MILESTONE 7: install handlers with sigaction(), not signal().
 *
 * signal() is older, and with this project's flags (-std=c11 and
 * _POSIX_C_SOURCE, so no _DEFAULT_SOURCE) glibc gives it System V
 * semantics: the handler is reset to the default after it runs once, so
 * the *second* Ctrl-C kills the shell. sigaction() spells out every
 * detail. See man 2 signal.
 *
 * TODO(M7): add <signal.h> and <string.h> (for memset) to the includes.
 *
 * TODO(M7): write a SIGINT handler, `static void on_sigint(int signo)`,
 *           that does as little as possible. A handler can interrupt the
 *           shell in the middle of printf() or malloc(), and calling either
 *           of those again from inside the handler can deadlock or corrupt
 *           their internal state. Only async-signal-safe functions are
 *           allowed (man 7 signal-safety); write() is on the list, printf()
 *           is not. An empty body is a perfectly good handler.
 *
 * TODO(M7): struct sigaction action; memset it to zero; set
 *           action.sa_handler = on_sigint; sigemptyset(&action.sa_mask);
 *           and call sigaction(SIGINT, &action, NULL), checking for -1.
 *           Leave SA_RESTART *out* of sa_flags for SIGINT: then a Ctrl-C at
 *           the prompt interrupts fgets() with EINTR, and read_line() in
 *           tinyshell.c already turns that into a fresh prompt.
 *
 * TODO(M7): write a SIGCHLD handler that only sets a flag:
 *               static volatile sig_atomic_t sigchld_seen = 0;
 *           volatile because it changes behind the compiler's back, and
 *           sig_atomic_t because it is the only type guaranteed to be
 *           written in one indivisible step. Install it with
 *           sa_flags = SA_RESTART | SA_NOCLDSTOP.
 *
 *           Do NOT call waitpid(-1, ...) in the handler. If SIGCHLD lands
 *           while the shell is waiting for a foreground command, the
 *           handler would reap *that* child and steal its exit status from
 *           the waitpid() in executor.c. Let ts_jobs_reap() do the reaping
 *           from the main loop, where it is safe.
 *
 * Test with: make test MILESTONE=7
 */
void ts_signals_install(void) {
}

/* TODO(M7): set SIGINT (and anything else you caught or ignored) back to
 *           SIG_DFL with sigaction(). A caught signal is reset by exec
 *           anyway, but an ignored one is not, and being explicit costs one
 *           line. This runs in the child, between fork and exec. */
void ts_signals_reset_for_child(void) {
}

/* TODO(M7): return whether sigchld_seen was set, and clear it. Until then
 *           the job table simply checks every time round the loop, which
 *           is correct, just less prompt. */
bool ts_signals_take_sigchld(void) {
  return true;
}
