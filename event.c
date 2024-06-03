#include "event.h"
#include "utils.h"

#define MAX_MOUNTS 100

static struct {
  Fsid fsid;
  int mount_fd;
} fsids[MAX_MOUNTS];

static size_t fsids_len;

void add_fsid(const char *mount_point) {
  if (fsids_len == MAX_MOUNTS) {
    warn("Too many mounts, not resolving fd paths for %s", mount_point);
    return;
  }

  int fd = open(mount_point, O_RDONLY | O_NOFOLLOW);
  if (fd < 0) {
    warn("Failed to open mount point %s", mount_point);
    return;
  }

  StatFs s;
  if (fstatfs(fd, &s) < 0) {
    warn("Failed to stat mount point %s", mount_point);
    close(fd);
    return;
  }

  memcpy(&fsids[fsids_len].fsid, &s.f_fsid, sizeof(s.f_fsid));
  fsids[fsids_len++].mount_fd = fd;
  debug("mount-> %s, fd-> %i", mount_point, fd);
}

int get_mount_id(const fsid_t *fsid) {
  for (size_t i = 0; i < fsids_len; ++i) {
    if (memcmp(fsid, &fsids[i].fsid, sizeof(fsids[i].fsid)) == 0) {
      debug("mapped fsid to fd %i", fsids[i].mount_fd);
      return fsids[i].mount_fd;
    }
  }

  debug("fsid not found, default to AT_FDCWD\n");
  return AT_FDCWD;
}

int get_fid_event_fd(const FanEventMetadata *data) {
  const FanEventInfoFid *fid = (const FanEventInfoFid *)(data + 1);

  if (fid->hdr.info_type != FAN_EVENT_INFO_TYPE_FID) {
    fatal("Received unexpected event info type %i", fid->hdr.info_type);

    exit(EXIT_FAILURE);
  }

  int fd = open_by_handle_at(get_mount_id((const Fsid *)&fid->fsid),
                             (FileHandle *)fid->handle,
                             O_RDONLY | O_NONBLOCK | O_LARGEFILE | O_PATH);

  if (fd < 0 && errno != ESTALE)
    warn("Failed open_by_handle_at with error code: %d", errno);

  return fd;
}

const char *op(uint64_t mask) {
  static char buffer[10];
  int offset = 0;

  if (mask & FAN_ACCESS)
    buffer[offset++] = 'R';
  if (mask & FAN_CLOSE_WRITE || mask & FAN_CLOSE_NOWRITE)
    buffer[offset++] = 'C';
  if (mask & FAN_MODIFY || mask & FAN_CLOSE_WRITE)
    buffer[offset++] = 'W';
  if (mask & FAN_OPEN)
    buffer[offset++] = 'O';
  if (mask & FAN_OPEN_EXEC)
    buffer[offset++] = 'E';
  if (mask & FAN_CREATE)
    buffer[offset++] = '+';
  if (mask & FAN_DELETE)
    buffer[offset++] = 'D';
  if (mask & FAN_MOVED_FROM)
    buffer[offset++] = '<';
  if (mask & FAN_MOVED_TO)
    buffer[offset++] = '>';
  if (mask & FAN_RENAME)
    buffer[offset++] = '|';
  if (mask & FAN_ATTRIB)
    buffer[offset++] = 'M';

  buffer[offset] = '\0';

  return buffer;
}

off_t size(const char *filename) {
  Stat st;
  stat(filename, &st);
  return st.st_size;
}

Event *next(const FanEventMetadata *data, time_t *event_time, bool client) {
  int event_fd = data->fd;
  static char buf[100];
  static char proc[100];
  static int procname_pid = -1;
  static char path[PATH_MAX];

  bool got_procname = false;
  Stat proc_fd_stat;

  if ((data->mask & 0xffffffff) == 0 || (data->pid == getpid())) {
    if (event_fd >= 0)
      close(event_fd);
    return NULL;
  }

  snprintf(buf, sizeof(buf), "/proc/%i", data->pid);
  int proc_fd = open(buf, O_RDONLY | O_DIRECTORY);
  if (proc_fd >= 0) {
    int procname_fd = openat(proc_fd, "comm", O_RDONLY);
    ssize_t len = read(procname_fd, proc, sizeof(proc));
    if (len >= 0) {
      while (len > 0 && proc[len - 1] == '\n')
        len--;
      proc[len] = '\0';
      procname_pid = data->pid;
      got_procname = true;
    } else {
      debug("failed to read /proc/%i/comm", data->pid);
    }

    close(procname_fd);

    if (fstat(proc_fd, &proc_fd_stat) < 0)
      debug("failed to stat /proc/%i: %m", data->pid);

    close(proc_fd);
  } else {
    debug("failed to open /proc/%i: %m", data->pid);
    proc[0] = '\0';
  }

  if (!got_procname) {
    if (data->pid == procname_pid) {
      debug("re-using cached procname value %s for pid %i", procname,
            procname_pid);
    } else if (procname_pid >= 0) {
      debug("invalidating previously cached procname %s for pid %i", procname,
            procname_pid);
      procname_pid = -1;
      proc[0] = '\0';
    }
  }

  const char *prog_comm = client ? "nfs" : "nfsd";

  if (strcmp(prog_comm, proc) != 0) {
    if (event_fd >= 0)
      close(event_fd);
    return NULL;
  }

  event_fd = get_fid_event_fd(data);

  if (event_fd >= 0) {
    snprintf(buf, sizeof(buf), "/proc/self/fd/%i", event_fd);
    ssize_t len = readlink(buf, path, sizeof(path));
    if (len < 0) {
      Stat st;
      if (fstat(event_fd, &st) < 0) {
        err("Failed to fstat, returned exit code: %d", errno);
        exit(EXIT_FAILURE);
      }
      snprintf(path, sizeof(path), "device %i:%i inode %ld\n", major(st.st_dev),
               minor(st.st_dev), st.st_ino);
    } else {
      path[len] = '\0';
    }

    close(event_fd);
  } else {
    snprintf(path, sizeof(path), "(deleted)");
  }

  Event *ev = (Event *)malloc(sizeof(Event));

  ev->client = client;
  ev->pid = data->pid;
  ev->uid = proc_fd_stat.st_uid;
  ev->gid = proc_fd_stat.st_gid;
  ev->size = path[0] != '\0' ? size(path) : 0;
  ev->time = event_time;

  strncpy(ev->proc_name, proc[0] == '\0' ? "unknown" : proc, sizeof(proc));
  strncpy(ev->path, path, PATH_MAX);
  strncpy(ev->op, op(data->mask), sizeof(ev->op));

  return ev;
}

void do_mark(int fan_fd, const char *dir, bool fatal) {
  uint64_t mask = FAN_ACCESS | FAN_MODIFY | FAN_OPEN | FAN_CLOSE | FAN_ONDIR |
                  FAN_EVENT_ON_CHILD | FAN_CREATE | FAN_DELETE | FAN_MOVE;

  if (fanotify_mark(fan_fd, FAN_MARK_ADD | FAN_MARK_FILESYSTEM, mask, AT_FDCWD,
                    dir) < 0) {
    if (fatal) {
      err("Failed to add watch for %s", dir);
      exit(EXIT_FAILURE);
    } else {
      warn("Failed to add watch for %s", dir);
    }
  }
}

void fan_setup(int fan_fd) {
  do_mark(fan_fd, "/", false);
  add_fsid("/");

  FILE *mounts = setmntent("/proc/self/mounts", "r");
  if (mounts == NULL) {
    err("Failed to open filesystem description");
    exit(EXIT_FAILURE);
  }

  Mntent *mount;
  while ((mount = getmntent(mounts)) != NULL) {
    if (mount->mnt_fsname == NULL || access(mount->mnt_fsname, F_OK) != 0 ||
        mount->mnt_fsname[0] != '/') {
      if (strcmp(mount->mnt_type, "zfs") != 0) {
        debug("ignore: fsname: %s dir: %s type: %s", mount->mnt_fsname,
              mount->mnt_dir, mount->mnt_type);
        continue;
      }
    }

    if (strcmp(mount->mnt_dir, "/") == 0)
      continue;

    debug("Added watch for mount %s (%s)", mount->mnt_dir, mount->mnt_type);
    do_mark(fan_fd, mount->mnt_dir, false);
    add_fsid(mount->mnt_dir);
  }

  endmntent(mounts);
}

void printEvent(const Event *event) {
  char buffer[80];

  struct tm *timeinfo;
  timeinfo = localtime(event->time);
  strftime(buffer, sizeof(buffer), "[%Y-%m-%d] (%H:%M:%S)", timeinfo);

  printf("%s %-5s(%d) [%d:%d]: %3s %s(%lu bytes)\n", buffer, event->proc_name,
         event->pid, event->uid, event->gid, event->op, event->path,
         event->size);
}
