CC=gcc
CFLAGS=-g -std=gnu99 -I. -I$(SQLITE)

.PHONY: check-env

all: vfsbench

vfsbench: vfsbench.c check-env
	$(CC) $(CFLAGS) $< -o $@ -c

clean:
	rm -f vfsbench *~

check-env:
ifndef SQLITE
	$(error SQLITE is undefined)
endif
