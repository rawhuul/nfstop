#include "store.h"
#include "utils.h"

#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>

#define DB_PATH                                                                \
  (getenv("NFSTOP_STORE") ? getenv("NFSTOP_STORE") : "/var/log/nfstop.db")

#define TABLE_STMT                                                             \
  "CREATE TABLE IF NOT EXISTS Events(proc_name TEXT, pid INTEGER, uid "        \
  "INTEGER, gid INTEGER, size INTEGER, op TEXT, path TEXT, time INTEGER);"

#define INSERT_STMT                                                            \
  "INSERT INTO Events (proc_name, pid, uid, gid, size, op, path, time) "       \
  "VALUES (?, ?, ?, ?, ?, ?, ?, ?);"

#define FETCH_STMT                                                             \
  "SELECT COUNT(*) as count, proc_name, pid, uid, gid, size, op, path, time "  \
  "FROM Events GROUP BY op, path HAVING COUNT(*) > 1 ORDER BY count DESC;"

#define FETCH_THRESHOLD 100

store store_open(bool daemon) {
  sqlite3 *db;

  int rc =
      daemon ? sqlite3_open_v2(DB_PATH, &db,
                               SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL)
             : sqlite3_open_v2(DB_PATH, &db, SQLITE_OPEN_READONLY, NULL);

  if (rc != SQLITE_OK) {
    err("Failed to open store: %s, error: %s", DB_PATH, sqlite3_errmsg(db));
    sqlite3_close(db);
    return NULL;
  }

  rc = sqlite3_exec(db, "PRAGMA journal_mode = WAL", 0, 0, NULL);
  if (rc != SQLITE_OK) {
    err("Failed to setup WAL mode for store: %s, error code: %d", DB_PATH,
        sqlite3_errcode(db));
    sqlite3_close(db);
    return NULL;
  }

  rc = sqlite3_exec(db, TABLE_STMT, 0, 0, NULL);

  if (rc != SQLITE_OK) {
    err("Failed to create table in store: %s, error code: %d", DB_PATH,
        sqlite3_errcode(db));
    store_close(db);
    return NULL;
  }

  return db;
}

int store_insert(store db, Event *event) {
  sqlite3_stmt *stmt;

  int rc = sqlite3_prepare_v2(db, INSERT_STMT, -1, &stmt, 0);

  if (rc != SQLITE_OK) {
    err("Failed to create statement in store: %s, error code: %d", DB_PATH,
        sqlite3_errcode(db));
    store_close(db);
    return 1;
  }

  sqlite3_bind_text(stmt, 1, event->proc_name, -1, SQLITE_STATIC);
  sqlite3_bind_int(stmt, 2, event->pid);
  sqlite3_bind_int(stmt, 3, event->uid);
  sqlite3_bind_int(stmt, 4, event->gid);
  sqlite3_bind_int64(stmt, 5, (event->size / 1024));
  sqlite3_bind_text(stmt, 6, event->op, -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 7, event->path, -1, SQLITE_STATIC);
  sqlite3_bind_int64(stmt, 8, (long int)*(event->time));

  rc = sqlite3_step(stmt);
  if (rc != SQLITE_DONE) {
    err("Failed to insert event in store: %s, error code: %d", DB_PATH,
        sqlite3_errcode(db));

    store_close(db);
    return 1;
  }

  sqlite3_finalize(stmt);

  return 0;
}

int store_show(store db, WINDOW *win) {
  sqlite3_stmt *stmt;
  int rc = sqlite3_prepare_v2(db, FETCH_STMT, -1, &stmt, NULL);

  if (rc != SQLITE_OK) {
    err("Failed to prepare statment for store: %s, error code: %s", DB_PATH,
        sqlite3_errmsg(db));
    return 1;
  }

  int x, y;
  getmaxyx(win, y, x);
  int i = 7;
  Event event;
  while (true) {
    if (i == y - 1)
      break;

    rc = sqlite3_step(stmt);

    if (rc == SQLITE_ROW) {

      unsigned long int count = sqlite3_column_int(stmt, 0);

      snprintf(event.proc_name, sizeof(event.proc_name), "%s",
               sqlite3_column_text(stmt, 1));

      event.pid = sqlite3_column_int(stmt, 2);
      event.uid = sqlite3_column_int(stmt, 3);
      event.gid = sqlite3_column_int(stmt, 4);
      event.size = sqlite3_column_int(stmt, 5);

      snprintf(event.op, sizeof(event.op), "%s", sqlite3_column_text(stmt, 6));
      snprintf(event.path, sizeof(event.path), "%s",
               sqlite3_column_text(stmt, 7));
      time_t time = sqlite3_column_int(stmt, 8);
      event.time = &time;

      mvwprintw(win, i, 2, "%-9lu %-15s %-10d %-10d %-10d %-10ld %-10s %-10s",
                count, event.proc_name, event.pid, event.uid, event.gid,
                event.size, event.op, event.path);
      i++;

    } else if (rc == SQLITE_DONE) {
      break;
    } else {
      err("Failed to fetch data from store: %s, error code: %d", DB_PATH,
          sqlite3_errcode(db));
      return 1;
    }
  }

  sqlite3_finalize(stmt);

  return 0;
}

int store_close(store db) {
  int rc = sqlite3_close(db);

  if (rc != SQLITE_OK) {
    err("Failed to close store %s, error code: %d", DB_PATH,
        sqlite3_errcode(db));
    return 1;
  }

  return 0;
}
