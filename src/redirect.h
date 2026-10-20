#pragma once

#include "parser.h"

/*
 * redirect.h
 * MILESTONE 4: making `>`, `>>`, `<` and `2>` work.
 *
 * Read docs/src/milestones/m04-redirections.md first.
 */

/* Apply every redirect in `command` to this process's file descriptors.
 *
 * Call this in the *child*, after fork() and before exec. The child's fd table
 * is its own copy, so whatever it rewires vanishes with it and the shell's own
 * stdin and stdout are never touched. It's also why external commands need no
 * "restore" step afterwards.
 *
 * Returns 0 on success. On failure prints an error and returns -1; the
 * caller should then _exit() without running the program, the same way
 * bash refuses to run `cat < missing-file`.
 */
int ts_redirect_apply(const ts_command *command);
