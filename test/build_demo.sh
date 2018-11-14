#!/bin/bash

gcc -shared -I ../nfsv4 -fPIC -Wl,-soname,simpletest -o simpletest.so simpletest.c
LIBRARY_PATH=. gcc -shared -I ../nfsv4 -fPIC -Wl,-soname,simpletest -o simpletest.so -lnfs4 simpletest.c
