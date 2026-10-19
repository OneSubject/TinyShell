#pragma once

#include "tinyshell.h"

/*
 * builtins.h
 * Commands the shell runs itself, without forking.
 *
 * Why bother with builtins when /bin/cd exists? Because of what `cd` does.
 * A child process that calls chdir() changes its own directory and then exits,
 * and the parent is left exactly where it started. The shell is that parent, so
 * anything that has to change the shell's own state (the working directory, a
 * variable, the exit status) cannot be an external program, no matter what is
 * installed in PATH.
 *
 * That is also why this table has to be consulted before any fork(): once the
 * child exists, it is too late for a change to mean anything.
 */

typedef enum {
  TS_NOT_A_BUILTIN, /* no entry matches argv[0]; let the executor try PATH */
  TS_BUILTIN_OK,    /* a builtin ran; *status holds what it produced       */
  TS_BUILTIN_EXIT,  /* the shell should quit; *status is the exit code     */
} builtin_result;

/* Look up argv[0] in the builtin table and run it if it is there.
 *
 * Returns TS_NOT_A_BUILTIN when argv[0] is not a builtin, which is the only
 * case in which the caller should go on to fork. The three outcomes have to
 * be distinguishable: an earlier version returned the same value for "this
 * was not a builtin" and "this builtin ran fine", so the shell ran the
 * builtin and then forked a second time to exec the same name from PATH.
 *
 * *status is an out-parameter rather than a return value because a builtin
 * needs to report two things: a command to run (or not), and an exit status.
 * C functions return one thing; the second one comes back through a pointer.
 */
builtin_result ts_builtin_exec(char **argv, int *status);