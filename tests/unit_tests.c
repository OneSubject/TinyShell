/*
 * unit_tests.c
 * Tests for the parts of the shell that are plain functions.
 *
 * The lexer, the parser and PATH lookup take inputs and return outputs, so
 * they can be tested by calling them directly, which is faster and gives
 * far more precise failures than driving the whole shell. Everything that
 * forks, waits or touches signals is tested end to end by run_tests.sh
 * instead.
 *
 * No framework: one CHECK macro and a counter. When a check fails it prints
 * the file, line and the expression that was false, and carries on, so one
 * run shows every failure rather than only the first.
 */

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "parser.h"
#include "path.h"

static int checks_run = 0;
static int checks_failed = 0;

/* The do { } while (0) wrapper makes the macro behave like one statement,
 * so `if (x) CHECK(y); else ...` does what it looks like it does. */
#define CHECK(condition)                                                       \
  do {                                                                         \
    checks_run++;                                                              \
    if (!(condition)) {                                                        \
      checks_failed++;                                                         \
      fprintf(stderr, "  FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
    }                                                                          \
  } while (0)

#define CHECK_STR(actual, expected)                                            \
  do {                                                                         \
    const char *actual_ = (actual);                                            \
    const char *expected_ = (expected);                                        \
    checks_run++;                                                              \
    if (actual_ == NULL || strcmp(actual_, expected_) != 0) {                  \
      checks_failed++;                                                         \
      fprintf(stderr, "  FAIL %s:%d: %s is \"%s\", expected \"%s\"\n",         \
              __FILE__, __LINE__, #actual,                                     \
              actual_ != NULL ? actual_ : "(null)", expected_);                \
    }                                                                          \
  } while (0)

/* ts_parse() reports syntax errors on stderr, which is right for the shell
 * and noise in a test run. These two functions point fd 2 at /dev/null and
 * back, which is a small preview of MILESTONE 4: dup() keeps a copy of
 * where fd 2 used to go, dup2() puts it back. */
static int saved_stderr = -1;

static void silence_stderr(void) {
  fflush(stderr);
  saved_stderr = dup(STDERR_FILENO);
  int null_fd = open("/dev/null", O_WRONLY);
  if (null_fd != -1) {
    dup2(null_fd, STDERR_FILENO);
    close(null_fd);
  }
}

static void restore_stderr(void) {
  fflush(stderr);
  if (saved_stderr != -1) {
    dup2(saved_stderr, STDERR_FILENO);
    close(saved_stderr);
    saved_stderr = -1;
  }
}

/* lexer */

static void test_tokenize_words(void) {
  ts_token_list tokens;

  CHECK(ts_tokenize("  ls   -l\t/tmp  ", &tokens) == TS_LEX_OK);
  CHECK(tokens.count == 3);
  CHECK_STR(tokens.tokens[0].text, "ls");
  CHECK_STR(tokens.tokens[1].text, "-l");
  CHECK_STR(tokens.tokens[2].text, "/tmp");
  CHECK(!tokens.tokens[0].is_operator);
}

static void test_tokenize_empty_line(void) {
  ts_token_list tokens;

  CHECK(ts_tokenize("", &tokens) == TS_LEX_OK);
  CHECK(tokens.count == 0);
  CHECK(ts_tokenize("   \t ", &tokens) == TS_LEX_OK);
  CHECK(tokens.count == 0);
}

static void test_tokenize_operators_need_no_spaces(void) {
  ts_token_list tokens;

  CHECK(ts_tokenize("echo hi>out|wc -c", &tokens) == TS_LEX_OK);
  CHECK(tokens.count == 7);
  CHECK_STR(tokens.tokens[2].text, ">");
  CHECK(tokens.tokens[2].is_operator);
  CHECK_STR(tokens.tokens[3].text, "out");
  CHECK(!tokens.tokens[3].is_operator);
  CHECK_STR(tokens.tokens[4].text, "|");
  CHECK(tokens.tokens[4].is_operator);
}

static void test_tokenize_two_character_operators(void) {
  ts_token_list tokens;

  CHECK(ts_tokenize("cmd >>log 2>err", &tokens) == TS_LEX_OK);
  CHECK(tokens.count == 5);
  CHECK_STR(tokens.tokens[1].text, ">>");
  CHECK_STR(tokens.tokens[3].text, "2>");
  CHECK(tokens.tokens[3].is_operator);

  /* "2>" only at the start of a word: here the 2 belongs to "a2". */
  CHECK(ts_tokenize("echo a2>f", &tokens) == TS_LEX_OK);
  CHECK(tokens.count == 4);
  CHECK_STR(tokens.tokens[1].text, "a2");
  CHECK_STR(tokens.tokens[2].text, ">");
}

static void test_tokenize_too_many_tokens(void) {
  char line[TS_MAX_LINE];
  ts_token_list tokens;
  size_t used = 0;

  /* TS_MAX_ARGS words of "x " is one more than fits. */
  for (int i = 0; i < TS_MAX_ARGS; i++) {
    line[used++] = 'x';
    line[used++] = ' ';
  }
  line[used] = '\0';

  CHECK(ts_tokenize(line, &tokens) == TS_LEX_TOO_MANY_TOKENS);
}

static void test_tokenize_does_not_modify_line(void) {
  const char original[] = "a | b > c";
  char line[sizeof(original)];
  ts_token_list tokens;

  memcpy(line, original, sizeof(original));
  CHECK(ts_tokenize(line, &tokens) == TS_LEX_OK);
  CHECK(memcmp(line, original, sizeof(original)) == 0);
}

static void test_is_blank(void) {
  CHECK(ts_is_blank(""));
  CHECK(ts_is_blank(" \t\n"));
  CHECK(!ts_is_blank("  x "));
}

/* parser */

/* Lex and parse in one go; most parser tests only care about the result. */
static bool parse_line(const char *line, ts_token_list *tokens,
                       ts_pipeline *pipeline) {
  if (ts_tokenize(line, tokens) != TS_LEX_OK)
    return false;
  silence_stderr();
  bool ok = ts_parse(tokens, pipeline);
  restore_stderr();
  return ok;
}

static void test_parse_simple_command(void) {
  ts_token_list tokens;
  ts_pipeline pipeline;

  CHECK(parse_line("ls -l", &tokens, &pipeline));
  CHECK(pipeline.count == 1);
  CHECK(!pipeline.background);
  CHECK(pipeline.commands[0].argc == 2);
  CHECK_STR(pipeline.commands[0].argv[0], "ls");
  CHECK(pipeline.commands[0].argv[2] == NULL); /* execv needs the NULL */
  CHECK(pipeline.commands[0].redirect_count == 0);
}

static void test_parse_pipeline_and_background(void) {
  ts_token_list tokens;
  ts_pipeline pipeline;

  CHECK(parse_line("cat f | sort | uniq -c &", &tokens, &pipeline));
  CHECK(pipeline.count == 3);
  CHECK(pipeline.background);
  CHECK_STR(pipeline.commands[1].argv[0], "sort");
  CHECK(pipeline.commands[1].argv[1] == NULL);
  CHECK_STR(pipeline.commands[2].argv[1], "-c");
}

static void test_parse_redirects_leave_argv(void) {
  ts_token_list tokens;
  ts_pipeline pipeline;

  CHECK(parse_line("sort -r < in > out 2> err", &tokens, &pipeline));
  const ts_command *command = &pipeline.commands[0];
  CHECK(command->argc == 2);
  CHECK_STR(command->argv[1], "-r");
  CHECK(command->argv[2] == NULL);
  CHECK(command->redirect_count == 3);
  CHECK(command->redirects[0].kind == TS_REDIRECT_IN);
  CHECK_STR(command->redirects[0].path, "in");
  CHECK(command->redirects[1].kind == TS_REDIRECT_OUT);
  CHECK(command->redirects[2].kind == TS_REDIRECT_ERR);
  CHECK_STR(command->redirects[2].path, "err");
}

static void test_parse_syntax_errors(void) {
  ts_token_list tokens;
  ts_pipeline pipeline;

  CHECK(!parse_line("| ls", &tokens, &pipeline));
  CHECK(!parse_line("ls |", &tokens, &pipeline));
  CHECK(!parse_line("ls | | wc", &tokens, &pipeline));
  CHECK(!parse_line("ls >", &tokens, &pipeline));
  CHECK(!parse_line("ls > | wc", &tokens, &pipeline));
  CHECK(!parse_line("> out", &tokens, &pipeline));
  CHECK(!parse_line("sleep 1 & ls", &tokens, &pipeline));
  CHECK(!parse_line("&", &tokens, &pipeline));
}

/* PATH lookup (MILESTONE 3) */

static void write_file(const char *path, mode_t mode) {
  FILE *file = fopen(path, "w");
  if (file == NULL) {
    perror(path);
    exit(1);
  }
  fputs("#!/bin/sh\n", file);
  fclose(file);
  chmod(path, mode);
}

/* snprintf() into a fixed buffer, and stop the test run if it would not
 * fit. Checking the return value is also what tells gcc the truncation was
 * thought about; ignore it and -Wformat-truncation rightly complains. */
static void join_path(char *out, size_t size, const char *dir,
                      const char *name) {
  int written = snprintf(out, size, "%s/%s", dir, name);
  if (written < 0 || (size_t)written >= size) {
    fprintf(stderr, "unit tests: temporary path too long\n");
    exit(1);
  }
}

static void test_find_in_path(void) {
  char dir_template[] = "/tmp/tinyshell-unit.XXXXXX";
  char *dir = mkdtemp(dir_template);
  if (dir == NULL) {
    perror("mkdtemp");
    checks_failed++;
    return;
  }

  char first[TS_MAX_PATH], second[TS_MAX_PATH], path_value[3 * TS_MAX_PATH];
  char tool[TS_MAX_PATH], data[TS_MAX_PATH], shadow[TS_MAX_PATH];
  char found[TS_MAX_PATH];

  join_path(first, sizeof(first), dir, "first");
  join_path(second, sizeof(second), dir, "second");
  mkdir(first, 0755);
  mkdir(second, 0755);

  /* first/ has a *directory* called tool and a non-executable file called
   * data; second/ has the real, executable tool. */
  join_path(shadow, sizeof(shadow), first, "tool");
  join_path(data, sizeof(data), first, "data");
  join_path(tool, sizeof(tool), second, "tool");
  mkdir(shadow, 0755);
  write_file(data, 0644);
  write_file(tool, 0755);

  const char *old_path = getenv("PATH");
  char saved_path[TS_MAX_PATH] = "";
  if (old_path != NULL)
    snprintf(saved_path, sizeof(saved_path), "%s", old_path);

  snprintf(path_value, sizeof(path_value), "%s:%s", first, second);
  setenv("PATH", path_value, 1);

  /* A directory with the right name is not a program; keep looking. */
  CHECK(ts_find_in_path("tool", found, sizeof(found)) == 0);
  CHECK_STR(found, tool);

  /* Found, but not executable: EACCES, which becomes exit status 126. */
  errno = 0;
  CHECK(ts_find_in_path("data", found, sizeof(found)) == -1);
  CHECK(errno == EACCES);

  errno = 0;
  CHECK(ts_find_in_path("no-such-tool", found, sizeof(found)) == -1);
  CHECK(errno == ENOENT);

  /* A name with a slash is used as-is, without searching. */
  CHECK(ts_find_in_path("./tool", found, sizeof(found)) == 0);
  CHECK_STR(found, "./tool");

  /* ...and never overflows the buffer it is given. */
  char tiny[4];
  errno = 0;
  CHECK(ts_find_in_path("/usr/bin/env", tiny, sizeof(tiny)) == -1);
  CHECK(errno == ENAMETOOLONG);

  if (old_path != NULL)
    setenv("PATH", saved_path, 1);
  else
    unsetenv("PATH");

  unlink(tool);
  unlink(data);
  rmdir(shadow);
  rmdir(first);
  rmdir(second);
  rmdir(dir);
}

int main(void) {
  test_tokenize_words();
  test_tokenize_empty_line();
  test_tokenize_operators_need_no_spaces();
  test_tokenize_two_character_operators();
  test_tokenize_too_many_tokens();
  test_tokenize_does_not_modify_line();
  test_is_blank();
  test_parse_simple_command();
  test_parse_pipeline_and_background();
  test_parse_redirects_leave_argv();
  test_parse_syntax_errors();
  test_find_in_path();

  if (checks_failed > 0) {
    printf("unit tests: %d of %d checks failed\n", checks_failed, checks_run);
    return 1;
  }
  printf("unit tests: all %d checks passed\n", checks_run);
  return 0;
}
