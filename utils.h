#ifndef UTILS_H
#define UTILS_H

#include <err.h>

#ifdef DEBUG
#define debug(fmt, ...) fprintf(stderr, "DEBUG: " fmt "\n", ##__VA_ARGS__)
#else
#define debug(...)                                                             \
  {}
#endif

#define warn(fmt, ...) fprintf(stderr, "WARN: " fmt "\n", ##__VA_ARGS__)

#define err(fmt, ...) fprintf(stderr, "ERROR: " fmt "\n", ##__VA_ARGS__)

#define fatal(fmt, ...) fprintf(stderr, "FATAL: " fmt "\n", ##__VA_ARGS__)

#endif
