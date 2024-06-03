#ifndef STAT_H
#define STAT_H

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <ncurses.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  pid_t pid;
  char state;

  unsigned long int min_flt;
  unsigned long int maj_flt;

  double cpu;
  double mem;

  long int num_threads;

  long int priority;
  long int nice;

  unsigned long long start_time;
} ProcStat;

ProcStat *procstat(bool client);
void showStat(const ProcStat *info, WINDOW *win);

#ifdef __cplusplus
}
#endif

#endif
