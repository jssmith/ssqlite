include ../Makefile

all: shell

%.o : %.c
	gcc -g -I. -I.. -std=gnu99 $< -c

%.o : ../%.c
	gcc -g -I. -I.. -std=gnu99 $< -c

SHELLOBJ= shell.o 

shell: $(NFS4_OBJ) $(SHELLOBJ)
	cc -g $^ -lm -o shell

clean:
	rm -f *.o *~ shell
