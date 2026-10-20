#pragma once

#include <stddef.h>

#include "tinyshell.h"

/*
 * path.h
 * MILESTONE 3: finding a program on $PATH.
 *
 * When you type `ls`, nothing in the kernel knows what "ls" means. execv()
 * wants a path to a file. Turning a bare name into a path is the shell's job,
 * and the rule is simple: try each directory listed in $PATH, left to right,
 * and take the first one that holds an executable file of that name.
 *
 * execvp() can do this for you, and the shell used it until this milestone.
 * Writing it by hand is how you find out that `ls` and `./ls` are looked up
 * differently, why a directory called `ls` earlier in PATH does not break
 * anything, and what the difference between exit status 126 and 127 means.
 */

/* Resolve `name` to a path that can be passed to execv(), written to `out`.
 *
 * If `name` contains a '/', it is already a path and is copied as-is, with
 * no checks: `./script` means that file and nothing else. Otherwise each
 * directory in $PATH is tried in order. An empty entry in PATH (as in
 * "/bin::/usr/bin") means the current directory, as POSIX specifies.
 *
 * Returns 0 on success. On failure returns -1 and sets errno:
 *   ENOENT        no directory in PATH has a file by that name
 *   EACCES        one did, but it is not executable (status 126 territory)
 *   ENAMETOOLONG  `name` has a '/' and does not fit in `out`
 *
 * Returning -1 and setting errno is the convention every system call uses,
 * so callers can treat this exactly like one, including with ts_error().
 */
int ts_find_in_path(const char *name, char *out, size_t out_size);
