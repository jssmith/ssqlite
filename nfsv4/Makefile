
all: nfs4.so

SQLLITE = ../..

OBJ = rpc.o nfs4.o

.c.o: buffer.h nfs.h
	gcc -g -fPIC -I. -I$(SQLLITE) $< -c

nfs4.so: $(OBJ)
	ld -shared $(OBJ) -o nfs4.so

clean:
	rm -f $(OBJ) nfs4.so *~


