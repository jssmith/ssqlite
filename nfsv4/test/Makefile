#include ../Makefile

all: shell

OBJ = rpc.o xdr.o client.o 

%.o : %.c
	gcc -g -I. -I.. -std=gnu99 $< -c

%.o : ../%.c
	gcc -g -I. -I.. -std=gnu99 $< -c

SHELLOBJ= shell.o 

shell: $(OBJ) $(SHELLOBJ)
	cc -g $^ -lm -o shell

clean:
	rm -f *.o *~ shell
