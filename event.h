#ifndef NFSTOP_H
#define NFSTOP_H

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <dirent.h>
#include <fcntl.h>
#include <mntent.h>
#include <sys/fanotify.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/sysmacros.h>
#include <sys/types.h>

#define BUFSIZE 256 * 1024
#define MAX_MOUNTS 100

#ifdef DEBUG
#define debug(fmt, ...) fprintf(stderr, "DEBUG: " fmt "\n", ##__VA_ARGS__)
#else
#define debug(...)                                                             \
  {}
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef fsid_t Fsid;
typedef struct fanotify_event_metadata FanEventMetadata;
typedef struct fanotify_event_info_fid FanEventInfoFid;
typedef struct timeval TimeVal;
typedef struct stat Stat;
typedef struct mntent Mntent;
typedef struct statfs StatFs;
typedef struct file_handle FileHandle;

typedef struct {
  bool client;
  char proc_name[200];
  pid_t pid;
  uid_t uid;
  gid_t gid;
  off_t size;
  char op[10];
  char path[PATH_MAX];
  time_t *time;
} Event;

Event *next(const FanEventMetadata *data, time_t *event_time, bool client);
void printEvent(const Event *event);
void fan_setup(int fan_fd);

#ifdef __cplusplus
}
#endif

#endif
