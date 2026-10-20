#pragma once

#include "tinyshell.h"

/*
 * signals.h
 * MILESTONE 7: Ctrl-C should stop the program, not the shell.
 *
 * Read docs/src/milestones/m07-signals.md first.
 */

/* Install the shell's own signal handlers. Called once, from main(). */
void ts_signals_install(void);

/* Put the signal dispositions back to their defaults. Called in every child
 * between fork() and exec, so that Ctrl-C still kills `sleep 100` even
 * though the shell itself has stopped dying from it. */
void ts_signals_reset_for_child(void);

/* True if SIGCHLD arrived since the last call, and clears the flag. The job
 * table (MILESTONE 6) uses it to know there may be something to reap. */
bool ts_signals_take_sigchld(void);
