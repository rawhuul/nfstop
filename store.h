#ifndef DB_H
#define DB_H

#include "event.h"
#include "sqlite3.h"
#include <ncurses.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef sqlite3 *store;

typedef struct {
  unsigned long int count;
  Event ev;
} Row;

store store_open(bool daemon);
int store_insert(store db, Event *event);
int store_show(store db, WINDOW *win);
int store_close(store db);

#ifdef __cplusplus
}
#endif

#endif
