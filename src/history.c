/*
 * history.c
 * MILESTONE 10 (stretch): command history in a ring buffer.
 *
 * A ring buffer is a fixed array used as a queue that overwrites its oldest
 * entry when full. That suits history: the shell only ever cares about the
 * last N lines, and it never has to free anything.
 *
 *         next (where the 7th line will go)
 *          v
 *     [ 5 ][ 6 ][ 3 ][ 4 ]      capacity 4, 6 lines added so far:
 *                               the oldest surviving entry is line 3
 *
 * Arrow-key recall needs the terminal in raw mode (termios), which is a
 * project of its own; the GNU readline library does it for you, but it is not
 * POSIX, so it isn't used here. A `history` builtin that lists past commands
 * is the milestone.
 */

#include "history.h"

#include "error.h"

/* MILESTONE 10: history.
 *
 * TODO(M10): add <stdio.h> and <string.h>, and define the buffer:
 *                #define TS_HISTORY_SIZE 100
 *                static char entries[TS_HISTORY_SIZE][TS_MAX_LINE];
 *                static int total_added;   (lines ever added)
 *            The slot for line number n (counting from 0) is
 *            n % TS_HISTORY_SIZE.
 *
 * TODO(M10): ts_history_add(): copy the line into its slot with snprintf()
 *            (bounded, always terminated) and increment total_added.
 *
 * TODO(M10): ts_history_print(): the oldest surviving line is number
 *            max(0, total_added - TS_HISTORY_SIZE). Print from there to
 *            total_added - 1, as "%5d  %s\n" with 1-based numbers.
 *
 * Stretch on the stretch: save to ~/.tinyshell_history at exit and load it
 * at startup; or `!n` to re-run entry n.
 *
 * Test with: make test MILESTONE=10
 */
void ts_history_add(const char *line) {
  (void)line; /* TODO(M10) */
}

int ts_history_print(void) {
  ts_errorx("history: not implemented yet (MILESTONE 10)");
  return TS_STATUS_FAILURE;
}
