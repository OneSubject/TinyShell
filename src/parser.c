/*
 * parser.c
 * Tokenize a command line into argv.
 *
 * strtok is destructive, so the caller must pass a writable buffer.
 */

#include "parser.h"

#include <string.h>

/* Split `line` into tokens, writing pointers into `argv`.
 * Returns argc (number of tokens). argv is NULL-terminated.
 */
int ts_tokenize(char *line, char *argv[], int max_args) {
  int argc = 0;
  char *tokens = strtok(line, TS_DELIMS);

  while (tokens != NULL && argc < max_args - 1) {
    argv[argc++] = tokens;
    tokens = strtok(NULL, TS_DELIMS);
  }

  argv[argc] = NULL;
  return argc;
}

/* Return true if the line is empty or only whitespace. */
bool ts_is_blank(const char *line) {
  return strspn(line, TS_DELIMS) == strlen(line);
}
