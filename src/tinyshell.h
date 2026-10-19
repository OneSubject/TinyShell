#pragma once

#include <stdbool.h>
#include <stddef.h>

#define TS_MAX_LINE 4096 /* max chars on a command line      */
#define TS_MAX_ARGS 128  /* max argv slots (incl. NULL)      */
#define TS_DELIMS " \t\r\n\a"
#define TS_PROMPT "> "

#define TS_CONTINUE 1 /* keep the REPL going              */
#define TS_STOP 0     /* break out of the REPL            */
