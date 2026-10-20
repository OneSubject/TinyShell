#pragma once

#include "tinyshell.h"

/*
 * history.h
 * MILESTONE 10 (stretch): remembering what was typed.
 */

/* Remember one input line. Called by the REPL for every non-blank line,
 * before it runs. */
void ts_history_add(const char *line);

/* Print the remembered lines to stdout, oldest first, numbered from 1.
 * Returns an exit status for the `history` builtin. */
int ts_history_print(void);
