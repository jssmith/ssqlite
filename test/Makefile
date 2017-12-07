CC=gcc
CFLAGS=-g -std=gnu99 -I. -I$(SQLITE)

all: vfsbench

vfsbench: vfsbench.c
	$(CC) $(CFLAGS) $< -o $@ -c

clean:
	rm -f vfsbench *~

