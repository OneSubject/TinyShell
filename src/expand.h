#pragma once

#include <stddef.h>

#include "tinyshell.h"

/*
 * expand.h
 * MILESTONE 9: $VAR and $? expansion.
 *
 * The lexer (parser.c) finds a '$' and the name after it; this module
 * answers "what is that name's value right now". Keeping the lookup here
 * means the lexer never has to know where values come from.
 */

/* Remember the most recent exit status, for $?. Called by the REPL after
 * every command. */
void ts_expand_set_last_status(int status);

/* The value of variable `name`, which is `name_length` bytes long and is
 * *not* NUL-terminated (it points straight into the input line).
 *
 * Returns the value, or "" if the variable is not set: in a shell an unset
 * variable expands to nothing, it is not an error. The returned string must
 * not be modified or freed, and is only valid until the next call. */
const char *ts_expand_variable(const char *name, size_t name_length);
