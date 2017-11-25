
all: nfs4.so

OBJ = rpc.o nfs4.o

.c.o: buffer.h nfs.h check-env
	gcc -g -fPIC -I. -std=gnu99 -I$(SQLITE) $< -c

nfs4.so: $(OBJ)
	ld -shared $(OBJ) -o nfs4.so

clean:
	rm -f $(OBJ) nfs4.so *~

check-env:
ifndef SQLITE
    $(error SQLITE is undefined)
endif
