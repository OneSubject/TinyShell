/*
 * builtins.c
 * The builtin command table.
 *
 * Every builtin has the same signature, which is what makes the dispatch loop
 * at the bottom possible: given argv, do the thing, report a status. The loop
 * does not know which entry it called and neither do the entries.
 *
 * Builtins run in the shell's own address space. That is the entire point
 * and also the constraint: no fork has happened yet, so nothing here may
 * wait for a child, and any state a builtin changes is a state the user can
 * observe afterwards.
 */

#include "builtins.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef builtin_result (*builtin_fn)(char **argv, int *status);

/* The table. Adding a builtin is one line here and one function above; no
 * other file in the project has to learn the new command's name. */
typedef struct {
  const char *name;
  builtin_fn func;
} builtin_t;

/* cd: exercise 1, deliberately not written yet.
 *
 * The shape of the answer is in man 2 chdir: call it, then check errno. The
 * trap here is that $HOME is not guaranteed to exist or to be set, and that
 * `cd` with no argument is the one case that has to look somewhere other than
 * argv[1] (man 7 environ covers where a process gets its environment).
 */
static builtin_result builtin_cd(char **argv, int *status) {
  (void)argv;
  (void)status;
  fprintf(stderr, "tinyshell: cd: not implemented yet (README, step 1)\n");
  return TS_BUILTIN_OK;
}

/* exit [n]
 *
 * `exit` is here mostly as a demonstration of what the dispatch is for: it
 * has to influence the shell's own control flow, which is the same reason
 * `cd` cannot be external.
 *
 * With no argument it quits the way the last command finished. *status
 * arrives pre-loaded with that value (see shell_step()) and is only
 * overwritten here when an argument is actually given.
 */
static builtin_result builtin_exit(char **argv, int *status) {
  if (argv[1] != NULL && argv[2] != NULL) {
    fprintf(stderr, "tinyshell: exit: too many arguments\n");
    return TS_BUILTIN_OK;
  }

  if (argv[1] != NULL) {
    char *end = NULL;

    errno = 0; /* strtol reports failure through errno, and it may be left
                * over from an earlier unrelated call. */
    long code = strtol(argv[1], &end, 10);

    /* Three ways for this to be wrong: out of range (errno), not a number at
     * all (end never moved), or a number followed by junk (*end != '\0'). */
    if (errno != 0 || end == argv[1] || *end != '\0') {
      fprintf(stderr, "tinyshell: exit: '%s': not a number\n", argv[1]);
      return TS_BUILTIN_OK;
    }

    /* A process can only hand eight bits back to its parent. bash does the
     * same truncation rather than rejecting the value. */
    *status = (int)(code & 0xFF);
  }

  return TS_BUILTIN_EXIT;
}

static const builtin_t builtins[] = {
  {"cd", builtin_cd},
  {"exit", builtin_exit},
};

#define BUILTIN_COUNT (sizeof(builtins) / sizeof(builtins[0]))

builtin_result ts_builtin_exec(char **argv, int *status) {
  if (argv == NULL || argv[0] == NULL)
    return TS_NOT_A_BUILTIN;

  /* A linear scan over a handful of entries, by name, with strcmp. The whole
   * table fits in a couple of cache lines; a hash table here would cost more
   * than it saves and would bury the idea. If this list ever reaches a few
   * hundred entries that is the moment to change it. */
  for (size_t i = 0; i < BUILTIN_COUNT; i++) {
    if (strcmp(argv[0], builtins[i].name) == 0)
      return builtins[i].func(argv, status);
  }

  return TS_NOT_A_BUILTIN; /* not ours; the executor gets a turn */
}