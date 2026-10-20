/*
 * redirect.c
 * MILESTONE 4: redirections.
 *
 * A program does not know or care where its output goes. It writes to file
 * descriptor 1, and fd 1 is whatever the process inherited. Redirection is
 * the shell swapping what fd 1 refers to *before* the program starts:
 *
 *     before                     after open() + dup2(fd, 1) + close(fd)
 *     0 -> terminal              0 -> terminal
 *     1 -> terminal              1 -> out.txt
 *     2 -> terminal              2 -> terminal
 *     3 -> out.txt  (from open)  3 -> (closed)
 *
 * The parser has already done the string work: by the time this runs, the
 * redirect tokens have been removed from argv and are waiting in
 * command->redirects, in the order they were typed.
 */

#include "redirect.h"

#include "error.h"

/* MILESTONE 4: redirections.
 *
 * For each redirect, left to right:
 *
 * TODO(M4): pick the open() flags and the fd being replaced:
 *             TS_REDIRECT_IN      O_RDONLY                       -> fd 0
 *             TS_REDIRECT_OUT     O_WRONLY | O_CREAT | O_TRUNC   -> fd 1
 *             TS_REDIRECT_APPEND  O_WRONLY | O_CREAT | O_APPEND  -> fd 1
 *             TS_REDIRECT_ERR     O_WRONLY | O_CREAT | O_TRUNC   -> fd 2
 *           (STDIN_FILENO, STDOUT_FILENO and STDERR_FILENO from
 *           <unistd.h> are nicer names for 0, 1 and 2.)
 *
 * TODO(M4): fd = open(redirect->path, flags, 0644). The third argument is
 *           the permission for a *newly created* file and is required
 *           whenever O_CREAT is set; leaving it out reads garbage off the
 *           stack. On -1, ts_error("%s", redirect->path) and return -1.
 *
 * TODO(M4): dup2(fd, target) makes target refer to the same open file.
 *           Check it for -1 too.
 *
 * TODO(M4): close(fd). The file stays open through `target`. Forgetting
 *           this leaks a descriptor into the program you exec, which is
 *           harmless for one command and the cause of mysterious hangs
 *           once pipes are involved (MILESTONE 5).
 *
 * Applying them in order is what makes `cmd > a > b` create both files and
 * write only to b, the same as bash.
 *
 * Stretch: builtins run inside the shell, so `cd /tmp > out` would change
 * the *shell's* fd 1. Supporting that means saving the original with
 * dup(1), redirecting, running the builtin, then dup2() the saved copy back.
 * Until then tinyshell.c refuses redirects on builtins.
 *
 * Test with: make test MILESTONE=4
 */
int ts_redirect_apply(const ts_command *command) {
  if (command->redirect_count == 0)
    return 0;

  /* Placeholder until the TODOs above are done. Refusing is better than
   * silently ignoring the redirect and printing to the terminal: the user
   * asked for their output to go somewhere specific. */
  ts_errorx("%s: redirection is not implemented yet (MILESTONE 4)",
            command->redirects[0].path);
  return -1;
}
