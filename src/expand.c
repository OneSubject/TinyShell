/*
 * expand.c
 * MILESTONE 9: variable lookup.
 *
 * Environment variables are not a shell invention. Every process has an
 * environment, a list of NAME=value strings that it inherited from its parent
 * and will pass on to its children through exec. getenv(), setenv() and
 * unsetenv() read and change *this process's* copy. So `export` has to be a
 * builtin: a child running setenv() would change only its own copy, which
 * disappears when it exits (the same argument as cd).
 */

#include "expand.h"

#include <stdio.h>
#include <string.h>

/* "$?" as text. Kept as a string, formatted once per command, so that
 * ts_expand_variable() can return a pointer to it like any other value. */
static char last_status_text[16] = "0";

void ts_expand_set_last_status(int status) {
  snprintf(last_status_text, sizeof(last_status_text), "%d", status);
}

/* MILESTONE 9: look up a variable.
 *
 * TODO(M9): getenv() wants a NUL-terminated name, but `name` points into
 *           the middle of the user's line. Copy it into a local buffer
 *           first, and check name_length against the buffer's size before
 *           copying: a 5000-character variable name must not overflow it.
 *           (Return "" for names that do not fit; no such variable exists.)
 *
 * TODO(M9): add <stdlib.h>, call getenv(buffer), and return "" when it gives
 *           back NULL.
 *
 * TODO(M9): then wire it in: the '$' case in ts_tokenize() (parser.c), and
 *           the export/unset builtins in builtins.c.
 *
 * Test with: make test MILESTONE=9
 */
const char *ts_expand_variable(const char *name, size_t name_length) {
  if (name_length == 1 && name[0] == '?')
    return last_status_text;

  return ""; /* TODO(M9): environment lookup, as above */
}
