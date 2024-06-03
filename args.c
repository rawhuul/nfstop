#include "args.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>

Args *get_args(int argc, char *argv[]) {
  Args *args = (Args *)malloc(sizeof(Args));

  if (args == NULL) {
    err("Failed to allocate args");
    return NULL;
  }

  args->client = false;

  int opt;

  while ((opt = getopt(argc, argv, "cdhv")) != -1) {
    switch (opt) {
    case 'v':
#ifndef VERSION
      printf("nfstop (dev-build)\n");
#else
      printf("nfstop (v%s)\n", VERSION);
#endif
      return NULL;
    case 'h':
      printf("Usage: %s [options]\n", argv[0]);
      printf("Options:\n");
      printf("  -h, --help     Display this help message\n");
      printf("  -d, --daemon   Run as a daemon and writes to database, can be "
             "configured by environment variabel NFSTOP_STORE(default: "
             "/var/log/nfstop.log)\n");
      free(args);
      return NULL;
    case 'd':
      args->daemon = true;
      break;
    case 'c':
      args->client = true;
      break;
    default:
      fprintf(stderr, "Usage: %s [-h] [-d]\n", argv[0]);
      free(args);
      return NULL;
    }
  }

  return args;
}
