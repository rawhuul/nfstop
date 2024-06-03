#include "stat.h"
#include "utils.h"

#include <ncurses.h>
#include <sys/sysinfo.h>

pid_t pidof(bool client) {
  const char *comm = client ? "nfs" : "nfsd";

  char path[PATH_MAX];
  char cmd[256];

  DIR *dir = opendir("/proc");
  if (dir == NULL) {
    err("Failed to open /proc");
    return -1;
  }

  struct dirent *ent;
  while ((ent = readdir(dir)) != NULL) {
    if (atoi(ent->d_name) != 0) {
      snprintf(path, sizeof(path), "/proc/%s/comm", ent->d_name);

      int fd = open(path, O_RDONLY);
      if (fd == -1) {
        err("Error opening comm file: %d", errno);
        continue;
      }

      ssize_t len = read(fd, cmd, sizeof(cmd) - 1);
      if (len == -1) {
        err("Error reading comm file: %d", errno);
        continue;
      }

      close(fd);

      while (len > 0 && cmd[len - 1] == '\n')
        len--;

      cmd[len] = '\0';

      if (strcmp(cmd, comm) == 0) {
        closedir(dir);
        return atoi(ent->d_name);
      }
    }
  }

  closedir(dir);
  return -1;
}

ProcStat *procstat(bool client) {
  pid_t pid = pidof(client);

  if (pid == -1) {
    fatal("Process: %s doesn't exist", client ? "nfs" : "nfsd");
    return NULL;
  }

  ProcStat *ps = malloc(sizeof(ProcStat));
  ps->pid = pid;

  char filename[256];
  snprintf(filename, sizeof(filename), "/proc/%d/stat", pid);

  int fd = open(filename, O_RDONLY);
  if (fd == -1) {
    err("Failed to open stat file with error code: %d", errno);
    return NULL;
  }

  char buffer[256];
  ssize_t len = read(fd, buffer, sizeof(buffer) - 1);
  if (len == -1) {
    err("Failed to read stat file with error code: %d", errno);
    return NULL;
  }

  close(fd);

  while (len > 0 && buffer[len - 1] == '\n')
    len--;

  buffer[len] = '\0';

  unsigned long int utime = 0;
  unsigned long int stime = 0;
  unsigned long int cutime = 0;
  unsigned long int cstime = 0;

  unsigned long int vsize = 0;
  long int rss = 0;

  sscanf(buffer,
         "%*d %*s %c %*d %*d %*d %*d %*d %*u %lu %*u %lu %*u %lu %lu %lu %lu "
         "%ld %ld %ld %*d %llu %lu %ld",
         &ps->state, &ps->min_flt, &ps->maj_flt, &utime, &stime, &cutime,
         &cstime, &ps->priority, &ps->nice, &ps->num_threads, &ps->start_time,
         &vsize, &rss);

  struct sysinfo info;
  sysinfo(&info);
  ps->mem = ((double)rss * getpagesize()) / info.totalram * 100.0;

  unsigned long total_time = utime + stime + cutime + cstime;

  unsigned long sec = info.uptime - (ps->start_time / sysconf(_SC_CLK_TCK));

  ps->cpu = ((double)total_time / sysconf(_SC_CLK_TCK)) / sec * 100;

  return ps;
}

void showStat(const ProcStat *info, WINDOW *win) {
  struct sysinfo sys_info;
  sysinfo(&sys_info);
  unsigned long long sec = (info->start_time / sysconf(_SC_CLK_TCK));
  unsigned long long up_sec = sys_info.uptime - sec;

  mvwprintw(win, 1, 2, "PID: %d, start time - %llu, up - %llu", info->pid, sec,
            up_sec);
  mvwprintw(win, 2, 2, "state: %c", info->state);
  mvwprintw(win, 3, 2, "%%Cpu(s):  %.2f%%, %ld nice, %ld priority, %ld thread",
            info->cpu, info->nice, info->priority, info->num_threads);
  mvwprintw(win, 4, 2, "%%Mem :  %.2f%%, %lu min_flt, %lu maj_flt", info->mem,
            info->min_flt, info->maj_flt);
}
