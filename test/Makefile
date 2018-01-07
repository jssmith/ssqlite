CC=gcc
CFLAGS=-g -std=gnu99 -O2 -I.
CFLAGS_SQLITE = -I$(SQLITE) -L$(SQLITE)/.libs -lsqlite3
#CFLAGS += -DTIMING_DETAIL=${TIMING_DETAIL}

.PHONY: check-env

all: vfsbench testnfsvfs nfsoptest

vfsbench: vfsbench.c check-env
	$(CC) $(CFLAGS) $(CFLAGS_SQLITE) -lpthread $< -o $@

testnfsvfs: testnfsvfs.c check-env
	$(CC) $(CFLAGS) $(CFLAGS_SQLITE) $< -o $@

nfsoptest: nfsoptest.c check-env
	$(CC) $(CFLAGS) -I../nfsv4 -L../nfsv4 -lnfs4 $< -o $@

clean:
	rm -f vfsbench testnfsvfs *~

check-env:
ifndef SQLITE
	$(error SQLITE is undefined)
endif
