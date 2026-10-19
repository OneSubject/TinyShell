#pragma once

#include "tinyshell.h"

/*
 * executor.h
 * Running external programs.
 */

/* Run argv[0] with the rest of argv as its arguments, and wait for it.
 *
 * Returns the exit status to report for this command, which is the number $?
 * will hold. A program that exited 0 returns 0; a program that exited 7 returns
 * 7; a program killed by signal N returns 128 + N; a name that is not a
 * builtin and not in PATH returns TS_STATUS_NOT_FOUND (127), which is also
 * what a real program that chose to exit 127 returns.
 *
 * That last collision is deliberate and harmless in a shell this size: the
 * user already has a command-not-found message on stderr, so the number is
 * only ever used as a truthy/falsey answer to "did that work".
 */
int ts_launch(char **argv);