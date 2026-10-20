/*
 * error.c
 * Consistent error messages on stderr.
 *
 * Two details here are easy to get wrong, and both have bitten real shells:
 *
 *   1. errno is a global that almost every library call is allowed to
 *      change, *including on success*. vsnprintf() may touch it. So the
 *      value has to be copied out first, before we call anything at all.
 *
 *   2. stderr is unbuffered, so every fprintf() to it is its own write().
 *      Printing the prefix, the message and the reason in three calls means
 *      three writes, and when two processes of a pipeline fail at once their
 *      pieces can interleave on the terminal. Formatting the message into a
 *      local buffer and printing it with one call keeps each line whole.
 */

#include "error.h"

#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/* Long enough for any message the shell produces about a line of
 * TS_MAX_LINE bytes. If a message is longer, vsnprintf() cuts it short
 * safely; an error message that is truncated is still an error message. */
#define TS_MAX_MESSAGE 1024

static void report(bool with_reason, int saved_errno, const char *format,
                   va_list args) {
  char message[TS_MAX_MESSAGE];

  if (vsnprintf(message, sizeof(message), format, args) < 0)
    message[0] = '\0'; /* an encoding error; print what we can */

  if (with_reason)
    fprintf(stderr, "tinyshell: %s: %s\n", message, strerror(saved_errno));
  else
    fprintf(stderr, "tinyshell: %s\n", message);
}

void ts_error(const char *format, ...) {
  int saved_errno = errno; /* first, before anything can change it */
  va_list args;

  va_start(args, format);
  report(true, saved_errno, format, args);
  va_end(args);

  errno = saved_errno;
}

void ts_errorx(const char *format, ...) {
  int saved_errno = errno;
  va_list args;

  va_start(args, format);
  report(false, saved_errno, format, args);
  va_end(args);

  errno = saved_errno;
}
