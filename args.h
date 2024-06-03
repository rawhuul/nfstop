#ifndef ARGPARSE_H

#define ARGPARSE_H

#include <getopt.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct option Option;

typedef struct {
  bool daemon;
  bool client;
} Args;

Args *get_args(int argc, char *argv[]);

#ifdef __cplusplus
}
#endif

#endif
