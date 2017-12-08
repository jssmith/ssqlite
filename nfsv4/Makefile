.PHONY: check-env

all: nfs4.so

OBJ = rpc.o xdr.o client.o
SQLITE_OBJ = nfs4.o $(OBJ)

# set -DTRACE on nfs4.c for operation printfs
#TRACING = -DTRACE

nfs4.o: nfs4.c
ifndef SQLITE
	echo 'SQLITE is undefined'
else
	gcc -g -fPIC -I. $(TRACING) -std=gnu99 -I$(SQLITE) $< -c
endif

# how to depend on buffer.h nfs.h 
%.o : %.c
	gcc -g -fPIC -I. -std=gnu99 $< -c

nfs4.so: $(SQLITE_OBJ)
	ld -shared $(SQLITE_OBJ) -o nfs4.so

clean:
	rm -f *.o nfs4.so *~

shell: $(OBJ)  shell.o
	cc -g $^ -o shell

 
