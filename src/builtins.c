/*
 * builtins.c
 * The builtin command table.
 *
 * Every builtin has the same signature, which is what makes the dispatch loop
 * at the bottom possible: given argv, do the thing, report a status. The loop
 * does not know which entry it called and neither do the entries.
 *
 * Builtins run in the shell's own address space. That's why they exist, and it
 * also limits what they can do: no fork has happened yet, so nothing here may
 * wait for a child, and any state a builtin changes is a state the user can
 * observe afterwards.
 */

#include "builtins.h"

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "error.h"
#include "history.h"

typedef builtin_result (*builtin_fn)(char **argv, int *status);

/* The table. Adding a builtin is one line here and one function above; no
 * other file in the project has to learn the new command's name. */
typedef struct {
  const char *name;
  builtin_fn func;
} builtin_t;

/* MILESTONE 1: cd [dir]
 *
 * The first builtin that does real work, and the clearest example of why
 * builtins exist at all. A child that calls chdir() changes its own
 * directory and then exits; the shell is the parent and does not move.
 *
 * With no argument, cd goes to $HOME. getenv() returns NULL when a variable
 * is not set at all, and "" when it is set to nothing; both mean there is
 * nowhere to go, and chdir("") would fail with a less helpful message.
 *
 * Things bash does that are left as extensions: `cd -` (needs $OLDPWD), and
 * keeping $PWD up to date with setenv() after each successful chdir.
 */
static builtin_result builtin_cd(char **argv, int *status) {
  if (argv[1] != NULL && argv[2] != NULL) {
    ts_errorx("cd: too many arguments");
    *status = TS_STATUS_FAILURE;
    return TS_BUILTIN_OK;
  }

  const char *target = argv[1];
  if (target == NULL) {
    target = getenv("HOME");
    if (target == NULL || target[0] == '\0') {
      ts_errorx("cd: HOME not set");
      *status = TS_STATUS_FAILURE;
      return TS_BUILTIN_OK;
    }
  }

  /* chdir() reports *why* it failed in errno: ENOENT, ENOTDIR, EACCES.
   * ts_error() appends strerror(errno), so the user sees "No such file or
   * directory" rather than a bare "cd failed". */
  if (chdir(target) == -1) {
    ts_error("cd: %s", target);
    *status = TS_STATUS_FAILURE;
    return TS_BUILTIN_OK;
  }

  *status = TS_STATUS_OK;
  return TS_BUILTIN_OK;
}

/* MILESTONE 2: exit [n]
 *
 * `exit` has to influence the shell's own control flow, which is the same
 * reason `cd` cannot be external.
 *
 * It does not call exit() itself. It returns TS_BUILTIN_EXIT and lets the
 * loop unwind back to main(), which returns the status. The result is the
 * same process exit, but any cleanup the shell grows later (saving history,
 * killing background jobs) has one place to live instead of being skipped.
 *
 * With no argument it quits the way the last command finished. *status
 * arrives pre-loaded with that value (see shell_step()) and is only
 * overwritten here when an argument is actually given.
 */
static builtin_result builtin_exit(char **argv, int *status) {
  if (argv[1] != NULL && argv[2] != NULL) {
    /* bash refuses and keeps running too: quitting the wrong way because
     * of a typo would be worse than not quitting. */
    ts_errorx("exit: too many arguments");
    *status = TS_STATUS_FAILURE;
    return TS_BUILTIN_OK;
  }

  if (argv[1] != NULL) {
    char *end = NULL;

    errno = 0; /* strtol reports failure through errno, and it may be left
                * over from an earlier unrelated call. */
    long code = strtol(argv[1], &end, 10);

    /* Three ways for this to be wrong: out of range (errno), not a number at
     * all (end never moved), or a number followed by junk (*end != '\0').
     * atoi() would have returned 0 for all three, and `exit abc` would have
     * reported success. */
    if (errno != 0 || end == argv[1] || *end != '\0') {
      ts_errorx("exit: '%s': not a number", argv[1]);
      *status = TS_STATUS_USAGE;
      return TS_BUILTIN_OK;
    }

    /* A process can only hand eight bits back to its parent, so 256 becomes
     * 0 and -1 becomes 255. bash does the same truncation rather than
     * rejecting the value. */
    *status = (int)(code & 0xFF);
  }

  return TS_BUILTIN_EXIT;
}

/* MILESTONE 9: export NAME=value, unset NAME
 *
 * TODO(M9): export: for each argument, find the '=' with strchr(). Without
 *           one, report a usage error. With one, the name is the part
 *           before it and the value the part after. setenv() wants them as
 *           two separate strings, so copy the name into a buffer (checking
 *           its length first). Check setenv()'s return value: it can fail
 *           with EINVAL (empty name) or ENOMEM.
 *
 * TODO(M9): unset: call unsetenv() for each argument.
 *
 * Both run in the shell's process, so the change is seen by every command
 * started afterwards. Try `export X=1` then `printenv X`.
 */
static builtin_result builtin_export(char **argv, int *status) {
  (void)argv;
  ts_errorx("export: not implemented yet (MILESTONE 9)");
  *status = TS_STATUS_FAILURE;
  return TS_BUILTIN_OK;
}

static builtin_result builtin_unset(char **argv, int *status) {
  (void)argv;
  ts_errorx("unset: not implemented yet (MILESTONE 9)");
  *status = TS_STATUS_FAILURE;
  return TS_BUILTIN_OK;
}

/* MILESTONE 10: history. The real work is in history.c. */
static builtin_result builtin_history(char **argv, int *status) {
  (void)argv;
  *status = ts_history_print();
  return TS_BUILTIN_OK;
}

static const builtin_t builtins[] = {
  {"cd", builtin_cd},
  {"exit", builtin_exit},
  {"export", builtin_export},
  {"unset", builtin_unset},
  {"history", builtin_history},
};

#define BUILTIN_COUNT (sizeof(builtins) / sizeof(builtins[0]))

/* A linear scan over a handful of entries, by name, with strcmp. The whole
 * table fits in a couple of cache lines; a hash table here would cost more
 * than it saves and would bury the idea. If this list ever reaches a few
 * hundred entries that is the moment to change it. */
static const builtin_t *find_builtin(const char *name) {
  if (name == NULL)
    return NULL;

  for (size_t i = 0; i < BUILTIN_COUNT; i++) {
    if (strcmp(name, builtins[i].name) == 0)
      return &builtins[i];
  }
  return NULL;
}

bool ts_is_builtin(const char *name) {
  return find_builtin(name) != NULL;
}

builtin_result ts_builtin_exec(char **argv, int *status) {
  if (argv == NULL)
    return TS_NOT_A_BUILTIN;

  const builtin_t *builtin = find_builtin(argv[0]);
  if (builtin == NULL)
    return TS_NOT_A_BUILTIN; /* not ours; the executor gets a turn */

  return builtin->func(argv, status);
}
