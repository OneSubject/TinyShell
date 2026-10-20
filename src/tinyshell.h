#pragma once

#include <stdbool.h>

/*
 * tinyshell.h
 * The limits and vocabulary shared by every part of the shell.
 *
 * Almost every bug in a small shell is a limit that was set once, in one
 * place, and then forgotten by the time the value reached the code that had
 * to respect it. So they all live here.
 *
 * Every limit here is a fixed-size array somewhere, on purpose. Fixed buffers
 * make every overflow check visible and local, and nothing has to be freed.
 * The price is that each one is a limit the user can hit, so hitting one is
 * always reported as an error and the input is never just cut short.
 */

/* Bytes per input line, newline included. The buffer is sized so that a full
 * line always fits; see read_line() in tinyshell.c for what happens when one
 * does not. */
#define TS_MAX_LINE 4096

/* Tokens per line, and argv slots per command. execv() takes a pointer to a
 * NULL-terminated array, so one argv slot is always spent on the terminator;
 * the lexer stops one short of this so that the parser never runs out. */
#define TS_MAX_ARGS 128

/* Bytes of token text the lexer may produce from one line. Quote removal
 * (MILESTONE 8) only shrinks a line, but $VAR expansion (MILESTONE 9) can
 * grow it, so the lexer writes into its own buffer instead of the input
 * line, and that buffer gets some headroom. */
#define TS_MAX_TOKEN_BYTES (TS_MAX_LINE * 2)

/* Commands in one pipeline (MILESTONE 5), redirections on one command
 * (MILESTONE 4), and background jobs alive at once (MILESTONE 6). */
#define TS_MAX_PIPELINE 16
#define TS_MAX_REDIRECTS 8
#define TS_MAX_JOBS 32

/* Bytes for a full path built during PATH lookup (MILESTONE 3). The same
 * number as PATH_MAX in <linux/limits.h>; defining it here keeps it next to
 * the other limits. */
#define TS_MAX_PATH 4096

/* Written to stderr, and only when stdin is a terminal. See prompt() in
 * tinyshell.c. */
#define TS_PROMPT "> "

/* What the REPL loop should do next. These describe the *loop*, not the
 * command: whether a command succeeded is reported separately as a real exit
 * status, which is the number $? holds. Conflating the two is a reliable way
 * to end up with a shell that cannot be scripted against. */
#define TS_CONTINUE 1 /* prompt again  */
#define TS_STOP 0     /* leave the loop */

/* Exit statuses the shell reports for problems it ran into itself. Every
 * other shell uses the same numbers, so a script written for bash behaves
 * here. 126 and 127 mean "the program did not run" rather than "the program
 * ran and disliked its arguments", which is the distinction POSIX is
 * drawing. */
#define TS_STATUS_OK 0
#define TS_STATUS_FAILURE 1          /* generic failure, e.g. cd to nowhere */
#define TS_STATUS_USAGE 2            /* syntax error, bad builtin usage     */
#define TS_STATUS_NOT_EXECUTABLE 126 /* found, could not be run            */
#define TS_STATUS_NOT_FOUND 127      /* not a builtin, not in PATH          */
#define TS_STATUS_SIGNAL_BASE 128    /* killed by signal N reports 128 + N  */
