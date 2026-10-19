#pragma once

#include <stdbool.h>

/*
 * tinyshell.h
 * The limits and vocabulary shared by every part of the shell.
 *
 * Almost every bug in a small shell is a limit that was set once, in one
 * place, and then forgotten by the time the value reached the code that had
 * to respect it. Keep this list short and keep it honest.
 */

/* Bytes per input line, newline included. The buffer is sized so that a full
 * line always fits; see read_line() in tinyshell.c for what happens when one
 * does not. */
#define TS_MAX_LINE 4096

/* argv slots, including the NULL terminator. execve() takes a pointer to a
 * NULL-terminated array, so one slot is always spent on the terminator. */
#define TS_MAX_ARGS 128

/* Written to stderr, not stdout. See prompt() in tinyshell.c. */
#define TS_PROMPT "> "

/* What the REPL loop should do next. These describe the *loop*, not the
 * command: whether a command succeeded is reported separately as a real exit
 * status, which is the number $? holds. Conflating the two is a reliable way
 * to end up with a shell that cannot be scripted against. */
#define TS_CONTINUE 1 /* prompt again  */
#define TS_STOP 0     /* leave the loop */

/* Exit statuses the shell reports for problems it ran into itself. Every
 * other shell uses the same numbers, so a script written for bash behaves
 * here. They also mean "the program did not run" rather than "the program ran
 * and disliked its arguments", which is the distinction POSIX is drawing. */
#define TS_STATUS_OK 0
#define TS_STATUS_NOT_EXECUTABLE 126 /* found in PATH, could not be run */
#define TS_STATUS_NOT_FOUND 127      /* not a builtin, not in PATH        */