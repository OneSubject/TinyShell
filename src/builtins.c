/*
 * builtins.c
 * Builtin commands: cd, exit.
 *
 * These are stubs for now. Fill them in properly.
 */

#include "builtins.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int builtin_cd(char **argv) {
  (void)argv;
  fprintf(stderr, "tinyshell: cd not implemented yet\n");
  return TS_CONTINUE;
}

static int builtin_exit(char **argv) {
  (void)argv;
  fprintf(stderr, "tinyshell: exit not implemented yet\n");
  return TS_STOP;
}

typedef int (*builtin_fn)(char **);

typedef struct {
  const char *name;
  builtin_fn func;
} builtin_t;

static const builtin_t builtins[] = {
    {"cd", builtin_cd},
    {"exit", builtin_exit},
};

#define BUILTIN_COUNT (sizeof(builtins) / sizeof(builtins[0]))

/* Return TS_STOP if a builtin handled the command and wants to quit,
 * TS_CONTINUE otherwise. Returns TS_CONTINUE if it's not a builtin at all.
 */
int ts_builtin_exec(char **argv) {
  if (argv == NULL || argv[0] == NULL)
    return TS_CONTINUE;

  for (size_t i = 0; i < BUILTIN_COUNT; i++) {
    if (strcmp(argv[0], builtins[i].name) == 0)
      return builtins[i].func(argv);
  }

  return TS_CONTINUE; /* not a builtin */
}
