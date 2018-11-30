CC=gcc
CFLAGS=-g -std=gnu99 -I. -I$(SQLITE) -L$(SQLITE)/.libs -lsqlite3
PY_INCLUDES=$(shell python3-config --includes)

.PHONY: check-env

all: vfsbench testnfsvfs nfsv4_read_demo nfsv4_write_demo

pysfs.so: pysfs.c
	gcc -fPIC -shared -Wall -lnfs4 -Wl,-soname,pysfs -g -std=gnu99 $(PY_INCLUDES) -I../nfsv4 -L../nfsv4 $< -o pysfs.so

nfsv4_read_write_test: nfsv4_read_write_test.c
	gcc -Wall -g -std=gnu99 -I../nfsv4 -L../nfsv4 -lnfs4 $< -o $@

vfsbench: vfsbench.c check-env
	$(CC) $(CFLAGS) $< -o $@

testnfsvfs: testnfsvfs.c check-env
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -f vfsbench testnfsvfs nfsv4_read_write_test pysfs.so *~

check-env:
ifndef SQLITE
	$(error SQLITE is undefined)
endif
