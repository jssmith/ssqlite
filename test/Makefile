CC=gcc
CFLAGS=-g -std=gnu99 -I. -I$(SQLITE) -L$(SQLITE)/.libs -lsqlite3

.PHONY: check-env

all: vfsbench testnfsvfs nfsv4_read_demo

nfsv4_read_demo: nfsv4_read_demo.c
	gcc -Wall -g -std=gnu99 -I../nfsv4 -L../nfsv4 -lnfs4 $< -o $@

vfsbench: vfsbench.c check-env
	$(CC) $(CFLAGS) $< -o $@

testnfsvfs: testnfsvfs.c check-env
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -f vfsbench testnfsvfs nfsv4_read_demo *~

check-env:
ifndef SQLITE
	$(error SQLITE is undefined)
endif
