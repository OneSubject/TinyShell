#pragma once

#include "parser.h"

/*
 * executor.h
 * Running external programs.
 */

/* Run a parsed command line: one program, a pipeline (MILESTONE 5), or a
 * background job (MILESTONE 6). Builtins are not handled here; the REPL
 * gives them their chance first.
 *
 * `command_line` is the original text, used only to describe background
 * jobs to the user.
 *
 * Returns the exit status to report for this command, which is the number $?
 * will hold. A program that exited 0 returns 0; a program that exited 7
 * returns 7; a program killed by signal N returns 128 + N; a name that is not
 * a builtin and not in PATH returns TS_STATUS_NOT_FOUND (127), which is also
 * what a real program that chose to exit 127 returns.
 *
 * That last overlap is harmless in a shell this size: the user already has a
 * command-not-found message on stderr, so the number is only ever used as a
 * truthy/falsey answer to "did that work".
 */
int ts_execute(const ts_pipeline *pipeline, const char *command_line);
