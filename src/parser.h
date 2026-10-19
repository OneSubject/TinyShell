#pragma once

#include "tinyshell.h"

/*
 * parser.h
 * Command-line tokenization interface.
 */

int ts_tokenize(char *line, char *argv[], int max_args);
bool ts_is_blank(const char *line);