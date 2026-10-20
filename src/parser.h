#pragma once

#include "tinyshell.h"

/*
 * parser.h
 * From a line of text to something the executor can run.
 *
 * This happens in two passes, and keeping them separate is what keeps each
 * one small:
 *
 *   lexing   "ls -l|wc>out"  ->  [ls] [-l] [|] [wc] [>] [out]
 *            characters become tokens. Quotes (MILESTONE 8) and $VAR
 *            (MILESTONE 9) are handled here, because this is the only pass
 *            that still knows which characters were inside quotes.
 *
 *   parsing  tokens  ->  pipeline of commands, each with argv and redirects
 *            operators like | and > stop being strings and become
 *            structure. Nothing here looks at individual characters.
 *
 * A quoted '|' has to reach a program as a plain argument, so each token
 * carries a flag saying whether it is an operator. That flag is the reason a
 * token is a struct and not just a char *.
 */

typedef struct {
  char *text;       /* NUL-terminated, points into ts_token_list.storage */
  bool is_operator; /* an unquoted | < > >> 2> or &                      */
} ts_token;

/* The lexer's output. The token text lives in `storage`, not in the input
 * line, so expansion can make a token longer than what was typed. Everything
 * is in one struct so it can be a single local variable with no malloc: when
 * the struct goes out of scope, every token goes with it. */
typedef struct {
  ts_token tokens[TS_MAX_ARGS];
  int count;
  char storage[TS_MAX_TOKEN_BYTES];
} ts_token_list;

typedef enum {
  TS_LEX_OK,
  TS_LEX_TOO_MANY_TOKENS,    /* more than TS_MAX_ARGS - 1 tokens       */
  TS_LEX_TOO_LONG,           /* token text overflowed `storage`        */
  TS_LEX_UNTERMINATED_QUOTE, /* MILESTONE 8: `echo 'oops` has no end   */
} ts_lex_result;

typedef enum {
  TS_REDIRECT_IN,     /* <  file   stdin  from file             */
  TS_REDIRECT_OUT,    /* >  file   stdout to file, truncate it  */
  TS_REDIRECT_APPEND, /* >> file   stdout to file, append       */
  TS_REDIRECT_ERR,    /* 2> file   stderr to file, truncate it  */
} ts_redirect_kind;

typedef struct {
  ts_redirect_kind kind;
  const char *path;
} ts_redirect;

/* One program to run: `sort -r < in > out` is argv {"sort", "-r", NULL}
 * plus two redirects. The redirect tokens never reach argv; sort must not
 * see a ">" among its arguments. */
typedef struct {
  char *argv[TS_MAX_ARGS]; /* NULL-terminated, ready for execv()        */
  int argc;
  ts_redirect redirects[TS_MAX_REDIRECTS]; /* applied left to right    */
  int redirect_count;
} ts_command;

/* `a | b | c &` is three commands and background = true. A plain `ls` is a
 * pipeline of one, which means the executor only ever has one shape to
 * deal with. */
typedef struct {
  ts_command commands[TS_MAX_PIPELINE];
  int count;
  bool background; /* trailing & (MILESTONE 6) */
} ts_pipeline;

/* Split `line` into tokens. `line` is not modified.
 *
 * On anything other than TS_LEX_OK the contents of `out` are unspecified and
 * the line should not be run: running half a command is worse than refusing
 * to run it. */
ts_lex_result ts_tokenize(const char *line, ts_token_list *out);

/* Group tokens into a pipeline. Prints a "syntax error" message and returns
 * false if the tokens do not form a valid command line.
 *
 * The pointers in `out` point into `tokens->storage`, so `tokens` must stay
 * alive for as long as `out` is in use. */
bool ts_parse(const ts_token_list *tokens, ts_pipeline *out);

/* True if the line holds nothing but whitespace. */
bool ts_is_blank(const char *line);
