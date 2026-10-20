#pragma once

#include <sys/types.h>

#include "tinyshell.h"

/*
 * jobs.h
 * MILESTONE 6: remembering background jobs, and reaping them.
 *
 * Read docs/src/milestones/m06-background-jobs.md, and the zombie section
 * of docs/src/concepts/processes.md, first.
 */

/* Record a background child. Returns its job number (the 1 in "[1] 4242"),
 * or -1 if the table is full. */
int ts_jobs_add(pid_t pid, const char *command_line);

/* Collect every background job that has finished, without blocking, and
 * report each one on stderr. Called at the top of every loop iteration, so
 * that no finished child stays a zombie for longer than one command. */
void ts_jobs_reap(void);
