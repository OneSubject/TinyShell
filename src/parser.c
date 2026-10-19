/*
 * parser.c
 * Split a command line into tokens.
 *
 * This is the first place where text becomes data. A line of characters goes
 * in, an array of strings comes out, and that array is exactly the shape a
 * C program expects to find in argv[]. Everything the shell does later is
 * built on top of this one array: builtins, redirection, pipes.
 *
 * We walk the buffer by hand rather than calling strtok(). strtok() would be
 * three lines shorter, but it has two habits that fight back:
 *
 *   1. it hides its cursor in a static variable, so two tokenizations cannot
 *      be live at the same time. A pipeline is one line producing several
 *      token arrays, and that rule starts working against you there;
 *
 *   2. it cannot represent an empty argument. `echo ""` is two words to a
 *      person and one argument to a program, but strtok() sees zero or one
 *      and collapses them. Every shell has to preserve that difference, so
 *      the function that defines the difference should be one we wrote.
 *
 * The tradeoff is real and worth stating out loud: this tokenizer has to
 * modify `line` to insert the '\0' characters that end each token. That is
 * fine here because the line was read into a buffer we own and will discard.
 * It would not be fine for a tokenizer over memory we do not own.
 */

#include "parser.h"

#include <ctype.h>
#include <stddef.h> /* NULL. Include the header for the thing you use, even
                     * when another one happens to drag it in. */

/* Split `line` into tokens and write pointers to them into `argv`.
 *
 * `line` is modified in place. `argv` is NULL-terminated and must have room
 * for `max_args` slots.
 *
 * Returns the number of tokens found. If the line holds more tokens than
 * argv has room for, the extras are dropped rather than misreported:
 * silently changing what a command means is worse than refusing to run it.
 */
int ts_tokenize(char *line, char *argv[], int max_args) {
  int argc = 0;
  char *cursor = line;

  while (*cursor != '\0' && argc < max_args - 1) {
    /* Step over the whitespace in front of the next word. */
    while (*cursor != '\0' && isspace((unsigned char)*cursor))
      cursor++;

    if (*cursor == '\0')
      break;

    /* The word starts here, and ends at the next whitespace. */
    argv[argc++] = cursor;
    while (*cursor != '\0' && !isspace((unsigned char)*cursor))
      cursor++;

    /* Eat the separator that ended the word, replacing it with the NUL byte
     * that terminates the token. If we already hit the end of the line there
     * is nothing to replace and the NUL is already where it belongs. */
    if (*cursor != '\0')
      *cursor++ = '\0';
  }

  argv[argc] = NULL;
  return argc;
}

/* True if the line holds nothing but whitespace.
 *
 * Worth checking before tokenizing: an empty argv[0] is not a command, and
 * every piece of code downstream would have to defend against one. Cheaper
 * never to produce it.
 *
 * isspace() rather than a hand-written list of delimiter characters, because
 * whitespace is a character class and the set of characters that count as it
 * is not identical on every platform.
 */
bool ts_is_blank(const char *line) {
  while (*line != '\0') {
    if (!isspace((unsigned char)*line))
      return false;
    line++;
  }
  return true;
}