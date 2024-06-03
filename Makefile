CC = gcc
CFLAG = -std=gnu99 -Wall

ifdef RELEASE 
	CFLAGS += -O3 -flto -fPIC -finline-functions -march=native -fstrict-aliasing -fomit-frame-pointer -ffast-math
else
	CFLAGS += -ggdb -O0 -std=gnu99 -Wall -Wextra 
endif

DFLAGS = -D_LARGEFILE64_SOURCE -D_GNU_SOURCE

ifdef RELEASE 
	DFLAGS += -DVERSION=\"0.1\"
endif

ifdef DEBUG
	DFLAGS += -DDEBUG 
endif

SQLITE_FLAGS = -DSQLITE_ENABLE_API_ARMOR -DSQLITE_OMIT_FOREIGN_KEY -DSQLITE_OMIT_EXPLAIN -DSQLITE_OMIT_MEMORYDB -DSQLITE_OMIT_DEPRECATED -DSQLITE_OMIT_DATETIME_FUNCS -DSQLITE_OMIT_BLOB_LITERAL
LDFLAGS = -lpthread -ldl -lm -lncurses
SRCS := $(filter-out test.c, $(wildcard *.c))
OBJS := $(SRCS:.c=.o)

.PHONY: all clean format

all: nfstop 

nfstop: $(OBJS)
	$(CC) $(CFLAGS) $(DFLAGS) $(SQLITE_FLAGS) $^ -o $@ $(LDFLAGS)

%.o: %.c 
	$(CC) $(CFLAGS) $(DFLAGS) -c $< -o $@

format:
	clang-format -i $(filter-out sqlite3.c sqlite3.h, $(wildcard *.c *.h))

clean:
	rm -f $(OBJS) nfstop 

