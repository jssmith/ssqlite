CC=gcc
CFLAGS=-g -std=gnu99 -I. -I$(SQLITE) -L$(SQLITE)/.libs -lsqlite3

.PHONY: check-env

all: vfsbench testnfsvfs

vfsbench: vfsbench.c check-env
	$(CC) $(CFLAGS) $< -o $@

testnfsvfs: testnfsvfs.c check-env
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -f vfsbench testnfsvfs *~

check-env:
ifndef SQLITE
	$(error SQLITE is undefined)
endif
