/*
 * path.c
 * MILESTONE 3: $PATH lookup.
 *
 * One thing to know before reading the code: the string getenv() returns
 * belongs to the environment, not to us. Writing to it changes $PATH for this
 * process and every child it starts later. That rules out strtok(), which
 * splits a string by writing '\0' into it. So the loop below walks PATH with
 * two pointers and never modifies it, and builds each candidate in the
 * caller's buffer instead.
 */

#include "path.h"

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* What the shell searches when PATH is not set at all. Some environments
 * (cron, `env -i`) start programs with an empty environment, and a shell
 * that cannot find `ls` there is unhelpful. confstr(_CS_PATH) asks glibc
 * for its default instead; a fixed string is easier to read. */
#define TS_DEFAULT_PATH "/usr/local/bin:/usr/bin:/bin"

typedef enum {
  CANDIDATE_MISSING,        /* nothing runnable by that name here */
  CANDIDATE_NOT_EXECUTABLE, /* a file is here, but no x bit       */
  CANDIDATE_EXECUTABLE,     /* found it                           */
} candidate_result;

static candidate_result check_candidate(const char *path) {
  struct stat info;

  if (stat(path, &info) == -1)
    return CANDIDATE_MISSING;

  /* A directory can have the x bit set (it means "may enter"), so access()
   * alone would happily report /usr/bin/ls as runnable if ls were a
   * directory. Only regular files are programs. */
  if (!S_ISREG(info.st_mode))
    return CANDIDATE_MISSING;

  /* access() checks against the real user id, which for a shell is the
   * same as the effective one. Good enough here; a setuid program would
   * need faccessat(..., AT_EACCESS). */
  if (access(path, X_OK) == -1)
    return CANDIDATE_NOT_EXECUTABLE;

  return CANDIDATE_EXECUTABLE;
}

int ts_find_in_path(const char *name, char *out, size_t out_size) {
  if (name == NULL || name[0] == '\0' || out_size == 0) {
    errno = ENOENT;
    return -1;
  }

  /* A slash means the user already gave us a path. Do not search, and do
   * not check: let execv() report what is wrong with it, because it knows
   * more than we do (ENOEXEC, ETXTBSY, ...). */
  if (strchr(name, '/') != NULL) {
    int written = snprintf(out, out_size, "%s", name);
    if (written < 0 || (size_t)written >= out_size) {
      errno = ENAMETOOLONG;
      return -1;
    }
    return 0;
  }

  const char *path = getenv("PATH");
  if (path == NULL)
    path = TS_DEFAULT_PATH;

  bool saw_not_executable = false;
  const char *dir_start = path;

  for (;;) {
    const char *dir_end = strchr(dir_start, ':');
    size_t dir_length =
      dir_end != NULL ? (size_t)(dir_end - dir_start) : strlen(dir_start);

    /* snprintf() never writes past out_size, but it returns how long the
     * result *would* have been. A return value >= out_size means the path
     * was cut short, and a truncated path is a different file. Such a
     * directory is skipped, not fatal: a later one may fit. */
    int written;
    if (dir_length == 0)
      written = snprintf(out, out_size, "./%s", name);
    else
      written = snprintf(out, out_size, "%.*s/%s", (int)dir_length,
                         dir_start, name);

    if (written >= 0 && (size_t)written < out_size) {
      candidate_result result = check_candidate(out);
      if (result == CANDIDATE_EXECUTABLE)
        return 0;
      if (result == CANDIDATE_NOT_EXECUTABLE)
        saw_not_executable = true;
    }

    if (dir_end == NULL)
      break;
    dir_start = dir_end + 1;
  }

  /* Keep searching after a non-executable match, because a later directory
   * may still have a runnable one. Only if none does is "permission denied"
   * the more useful thing to report than "not found". */
  errno = saw_not_executable ? EACCES : ENOENT;
  return -1;
}
