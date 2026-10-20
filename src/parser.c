/*
 * parser.c
 * Lexing and parsing a command line.
 *
 * This is the first place where text becomes data. A line of characters goes
 * in; a pipeline of commands comes out, each with an argv[] in exactly the
 * shape a C program expects to receive. Everything the shell does later is
 * built on that structure: builtins, redirection, pipes, background jobs.
 *
 * We walk the buffer by hand rather than calling strtok(). strtok() would be
 * shorter, but it has three habits that fight back:
 *
 *   1. it hides its cursor in a static variable, so two tokenizations cannot
 *      be live at the same time;
 *
 *   2. it cannot represent an empty argument. `echo ""` is one argument to a
 *      program, an empty string, and strtok() would drop it entirely;
 *
 *   3. it writes '\0' into the string it is splitting. Here that would be the
 *      user's line, and once $VAR expansion exists the tokens need to be
 *      *longer* than the line, so writing into it is impossible anyway.
 *
 * So the lexer copies characters one at a time into ts_token_list.storage,
 * and every single write is bounds-checked by append_char(). That function is
 * the only place in this file that writes into the buffer, which is what
 * makes it possible to convince yourself there is no overflow: you only have
 * to check that one function.
 */

#include "parser.h"

#include <ctype.h>
#include <string.h>

#include "error.h"

/* Lexer */

/* The lexer's working state. `result` records the first error; after that
 * every helper returns false and the main loop bails out. */
typedef struct {
  ts_token_list *out;
  size_t used;  /* bytes of out->storage written so far    */
  bool in_word; /* a token is open and accepting characters */
  ts_lex_result result;
} lexer;

/* Open a new token at the current end of storage. */
static bool start_token(lexer *lx, bool is_operator) {
  /* One argv slot is reserved for the NULL terminator, so a command can
   * never have more than TS_MAX_ARGS - 1 words. Capping the token count at
   * the same number means the parser can never overflow argv. */
  if (lx->out->count >= TS_MAX_ARGS - 1) {
    lx->result = TS_LEX_TOO_MANY_TOKENS;
    return false;
  }

  ts_token *token = &lx->out->tokens[lx->out->count++];
  token->text = &lx->out->storage[lx->used];
  token->is_operator = is_operator;
  lx->in_word = true;
  return true;
}

/* Append one byte to the open token. The only write into storage. */
static bool append_char(lexer *lx, char c) {
  /* `+ 1` keeps one byte free for the '\0' that end_token() writes. Without
   * it a token that exactly fills the buffer would lose its terminator, and
   * the next strlen() would walk off the end of the array. */
  if (lx->used + 1 >= sizeof(lx->out->storage)) {
    lx->result = TS_LEX_TOO_LONG;
    return false;
  }
  lx->out->storage[lx->used++] = c;
  return true;
}

/* Close the open token, if there is one, by terminating it. */
static bool end_token(lexer *lx) {
  if (!lx->in_word)
    return true;

  if (lx->used >= sizeof(lx->out->storage)) {
    lx->result = TS_LEX_TOO_LONG;
    return false;
  }
  lx->out->storage[lx->used++] = '\0';
  lx->in_word = false;
  return true;
}

/* How many characters of operator start at `cursor`, or 0 if none does.
 *
 * Operators end the word in front of them, so `ls>out` lexes the same as
 * `ls > out`. `2>` is only an operator at the start of a word: `echo 2>f`
 * redirects stderr, but `echo a2>f` writes "a2" to f, which is what bash
 * does too. */
static size_t operator_length(const char *cursor, bool in_word) {
  if (cursor[0] == '>' && cursor[1] == '>')
    return 2;
  if (cursor[0] == '2' && cursor[1] == '>' && !in_word)
    return 2;
  /* strchr() counts the string's own terminator as a match, so '\0' would
   * be "found" in "|&<>". Rule it out first. */
  if (cursor[0] != '\0' && strchr("|&<>", cursor[0]) != NULL)
    return 1;
  return 0;
}

ts_lex_result ts_tokenize(const char *line, ts_token_list *out) {
  lexer lx = {.out = out, .used = 0, .in_word = false, .result = TS_LEX_OK};
  const char *cursor = line;

  out->count = 0;

  while (*cursor != '\0') {
    char c = *cursor;

    /* Whitespace ends a word and is otherwise thrown away. isspace() takes
     * an int that must be representable as unsigned char (or EOF); passing
     * a plain char that happens to be negative, like a UTF-8 byte, is
     * undefined behaviour. Hence the cast. */
    if (isspace((unsigned char)c)) {
      if (!end_token(&lx))
        return lx.result;
      cursor++;
      continue;
    }

    size_t op_length = operator_length(cursor, lx.in_word);
    if (op_length > 0) {
      if (!end_token(&lx) || !start_token(&lx, true))
        return lx.result;
      for (size_t i = 0; i < op_length; i++) {
        if (!append_char(&lx, cursor[i]))
          return lx.result;
      }
      if (!end_token(&lx))
        return lx.result;
      cursor += op_length;
      continue;
    }

    /* MILESTONE 8: quoting and escaping.
     *
     * Right now ' " and \ are ordinary characters, so `echo "a b"` gives
     * echo two arguments, `"a` and `b"`. Shell quoting rules, in short:
     *
     *   'single'  everything up to the next ' is literal. No escapes, no $.
     *   "double"  literal, except that $ still expands (MILESTONE 9) and
     *             \ escapes the characters $ " \ (and a newline).
     *   \x        outside quotes: the next character is literal, whatever
     *             it is. `a\ b` is one word, `\|` is not a pipe.
     *
     * Quotes do not end a word: `ab"c d"e` is the single word `abc de`.
     *
     * TODO(M8): if c is '\'' or '"', open a word if none is open (an empty
     *           word is still a word: `echo ''` passes one empty argument,
     *           so start_token() must happen even if nothing is appended),
     *           then copy characters with append_char() until the matching
     *           quote. Reaching '\0' first means
     *           return TS_LEX_UNTERMINATED_QUOTE.
     * TODO(M8): if c is '\\', append the *next* character literally (if
     *           there is one) and skip both.
     * TODO(M8): characters appended this way are never operators, which
     *           is why operator_length() runs only on unquoted input.
     *
     * Test with: make test MILESTONE=8 */

    /* MILESTONE 9: $VAR expansion.
     *
     * TODO(M9): if c is '$' (outside single quotes), read the name that
     *           follows: either "?" or a run of letters, digits and '_'.
     *           Look it up with ts_expand_variable() from expand.h and copy
     *           the value in with append_char(). A lone '$' with no name
     *           after it stays a literal '$'.
     *
     * Test with: make test MILESTONE=9 */

    if (!lx.in_word && !start_token(&lx, false))
      return lx.result;
    if (!append_char(&lx, c))
      return lx.result;
    cursor++;
  }

  if (!end_token(&lx))
    return lx.result;
  return TS_LEX_OK;
}

/* Parser */

static void begin_command(ts_command *command) {
  command->argc = 0;
  command->argv[0] = NULL;
  command->redirect_count = 0;
}

/* Map an operator token to the redirect it stands for. Returns false if the
 * operator is not a redirect at all. */
static bool redirect_kind_for(const char *op, ts_redirect_kind *kind) {
  if (strcmp(op, "<") == 0)
    *kind = TS_REDIRECT_IN;
  else if (strcmp(op, ">") == 0)
    *kind = TS_REDIRECT_OUT;
  else if (strcmp(op, ">>") == 0)
    *kind = TS_REDIRECT_APPEND;
  else if (strcmp(op, "2>") == 0)
    *kind = TS_REDIRECT_ERR;
  else
    return false;
  return true;
}

bool ts_parse(const ts_token_list *tokens, ts_pipeline *out) {
  out->count = 1;
  out->background = false;
  ts_command *current = &out->commands[0];
  begin_command(current);

  for (int i = 0; i < tokens->count; i++) {
    const ts_token *token = &tokens->tokens[i];

    /* A plain word: one more argument. ts_tokenize() caps the token count
     * at TS_MAX_ARGS - 1, so this can never fill the last argv slot that
     * the NULL terminator needs. */
    if (!token->is_operator) {
      current->argv[current->argc++] = token->text;
      current->argv[current->argc] = NULL;
      continue;
    }

    const char *op = token->text;
    ts_redirect_kind kind;

    if (strcmp(op, "|") == 0) {
      if (current->argc == 0) {
        ts_errorx("syntax error near '|'");
        return false;
      }
      if (out->count == TS_MAX_PIPELINE) {
        ts_errorx("too many commands in pipeline (limit %d)",
                  TS_MAX_PIPELINE);
        return false;
      }
      current = &out->commands[out->count++];
      begin_command(current);
    } else if (strcmp(op, "&") == 0) {
      /* `&` only means something at the very end of the line. `a & b`
       * runs a in the background and b in the foreground in bash; that is
       * a fine extension once MILESTONE 6 works. */
      if (current->argc == 0 || i != tokens->count - 1) {
        ts_errorx("syntax error near '&'");
        return false;
      }
      out->background = true;
    } else if (redirect_kind_for(op, &kind)) {
      if (i + 1 >= tokens->count || tokens->tokens[i + 1].is_operator) {
        ts_errorx("syntax error: expected a file name after '%s'", op);
        return false;
      }
      if (current->redirect_count == TS_MAX_REDIRECTS) {
        ts_errorx("too many redirections (limit %d)", TS_MAX_REDIRECTS);
        return false;
      }
      ts_redirect *redirect = &current->redirects[current->redirect_count++];
      redirect->kind = kind;
      redirect->path = tokens->tokens[++i].text; /* consume the file name */
    } else {
      ts_errorx("syntax error near '%s'", op);
      return false;
    }
  }

  /* Every command needs a program name. `ls |` ends with an empty one, and
   * so does a line that is nothing but `> file`. */
  if (current->argc == 0) {
    ts_errorx("%s", out->count > 1 ? "syntax error: missing command after '|'"
                                   : "syntax error: missing command");
    return false;
  }
  return true;
}

/* True if the line holds nothing but whitespace.
 *
 * Worth checking before tokenizing: an empty argv[0] is not a command, and
 * every piece of code downstream would have to defend against one. Cheaper
 * never to produce it.
 *
 * isspace() rather than a hand-written list of delimiter characters, because
 * whitespace is a character class, and tabs, carriage returns and form feeds
 * are easy to forget in a list.
 */
bool ts_is_blank(const char *line) {
  while (*line != '\0') {
    if (!isspace((unsigned char)*line))
      return false;
    line++;
  }
  return true;
}
