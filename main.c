#include "args.h"
#include "event.h"
#include "stat.h"
#include "store.h"
#include "utils.h"

int collect_events(bool client) {
#ifndef DEBUG
  store db = store_open(true);

  if (db == NULL) {
    return 1;
  }
#endif

  int fan_fd = fanotify_init(FAN_CLASS_NOTIF | FAN_REPORT_FID, O_LARGEFILE);

  if (fan_fd < 0 && errno == EINVAL) {
    fatal("FAN_REPORT_FID not available");
    exit(EXIT_FAILURE);
  }

  if (fan_fd < 0)
    fan_fd = fanotify_init(0, O_LARGEFILE);

  if (fan_fd < 0) {
    fatal("Failed to initialize fanotify");
    exit(EXIT_FAILURE);
  }

  fan_setup(fan_fd);

  void *buffer = NULL;
  int res = posix_memalign(&buffer, 4096, BUFSIZE);
  if (res != 0 || buffer == NULL) {
    fatal("Failed to allocate buffer");
    exit(EXIT_FAILURE);
  }

  time_t event_time;

  FanEventMetadata *data;

  while (true) {
    res = read(fan_fd, buffer, BUFSIZE);
    if (res == 0) {
      warn("No more fanotify event (EOF)\n");
      break;
    }

    if (res < 0) {
      if (errno == EINTR)
        continue;
      err("Failed to read fanotify events, error code: %d", errno);
      exit(EXIT_FAILURE);
    }

    data = (FanEventMetadata *)buffer;

    while (FAN_EVENT_OK(data, res)) {
      if (data->vers != FANOTIFY_METADATA_VERSION) {

        fatal("Found discrepancy between fanotify metadata version");
        exit(EXIT_FAILURE);
      }

      time(&event_time);
      Event *event = next(data, &event_time, client);

      if (event != NULL) {
#ifndef DEBUG
        int rc = store_insert(db, event);

        if (rc != 0) {
          return rc;
        }
#else
        printEvent(event);
#endif

        free(event);
      }

      data = FAN_EVENT_NEXT(data, res);
    }
  }

#ifndef DEBUG
  return store_close(db);
#else
  return 0;
#endif
}

int main(int argc, char *argv[]) {
  Args *args = get_args(argc, argv);

  if (args == NULL) {
    return 0;
  } else if (args->daemon) {
    if (geteuid() != 0) {
      fatal("You need to run daemon with elevated privileges.");
      return 1;
    }

#ifndef DEBUG
    pid_t pid = fork();

    if (pid < 0) {
      fatal("Failed to run porcess in background, fork exited with code:%d.\n",
            errno);
      return 1;
    }

    if (pid > 0) {
      debug("Daemon ID: %d\n", pid);
      exit(0);
    }

    setsid();

    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
#endif

    int rc = collect_events(args->client);
    return rc;
  } else {
    sqlite3 *db = store_open(false);

    if (db == NULL) {
      return 1;
    }

    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);

    start_color();
    init_pair(1, COLOR_BLACK, COLOR_WHITE);

    WINDOW *win = newwin(0, 0, 0, 0);
    box(win, 0, 0);
    while (true) {
      ProcStat *p = procstat(args->client);

      if (p == NULL) {
        sleep(1);
        endwin();
        return 1;
      }

      clear();

      showStat(p, win);

      wattron(win, COLOR_PAIR(1));
      mvwprintw(win, 6, 2, "%-9s %-15s %-10s %-10s %-10s %-10s %-10s %-10s",
                "COUNT", "PROC_NAME", "PID", "UID", "GID", "SIZE", "OP",
                "PATH");
      wattroff(win, COLOR_PAIR(1));

      if (store_show(db, win) == 1) {
        break;
      }

      free(p);
      wrefresh(win);
      napms(1000);
    }
    endwin();
  }

  free(args);
  return 0;
}
