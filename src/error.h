#pragma once

/*
 * error.h
 * One way for the shell to complain.
 *
 * Every error message in TinyShell goes through these two functions, so they
 * all look the same:
 *
 *     tinyshell: cd: /nope: No such file or directory
 *     tinyshell: syntax error near '|'
 *
 * The "tinyshell:" prefix is useful once pipelines exist: three programs can
 * be writing to the same terminal at once, and the prefix is how the user
 * tells which of them is complaining.
 *
 * The pair mirrors err(3) and errx(3) from <err.h>: the plain name appends
 * strerror(errno), the -x variant does not. Use ts_error() right after a
 * system call fails, and ts_errorx() for problems the shell found by itself
 * (bad syntax, wrong number of arguments) where errno means nothing.
 */

/* Ask the compiler to check our format strings like it checks printf's.
 * Passing an int where the format says %s is undefined behaviour that
 * usually works, right up until it prints garbage or crashes. With this
 * attribute -Wall turns it into a compile-time warning instead. */
#if defined(__GNUC__)
#define TS_PRINTF_LIKE(format_index, first_arg_index)                          \
  __attribute__((format(printf, format_index, first_arg_index)))
#else
#define TS_PRINTF_LIKE(format_index, first_arg_index)
#endif

/* "tinyshell: <message>: <strerror(errno)>" on stderr.
 *
 * errno is read on entry, before anything else can overwrite it, and is left
 * unchanged on return so the caller can still inspect it. */
void ts_error(const char *format, ...) TS_PRINTF_LIKE(1, 2);

/* "tinyshell: <message>" on stderr, for errors that did not come from a
 * failed system call. */
void ts_errorx(const char *format, ...) TS_PRINTF_LIKE(1, 2);
