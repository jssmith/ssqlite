all: libnfs4.so

%.o : %.c
	gcc -g -fPIC -I. -std=gnu99 $< -c

NFS4_OBJ = rpc.o xdr.o client.o format.o nfs4_codepoints.o session.o xdrattr.o heap.o transfer.o

libnfs4.so: $(NFS4_OBJ)
	ld -shared $(NFS4_OBJ) -o libnfs4.so

clean:
	rm -f *.o nfs4.so libnfs4.so  *~
