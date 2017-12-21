This is an initial implementation of a userspace nfsv4.1 client implemented
as a sqlite vfs layer.

It currently only supports read only access.

steps to test:

  * create an aws efs instance, an ec2 instance, and follow the instructions to mount the
    filesystem on an instance, say in the /efs directory. note the hostname/ip address
    of the server for your region (i.e. 172.31.24.76)

  * download the sqlite 3.21 source, configure, and make

  * run sqlite, specifying a database file in /efs (i.e. ./sqlite3 /efs/db), and
    create a table (i.e. foo), and insert something into it, close sqlite
  
  * make  - be sure that the path to SQLITE in the top of makefile points
    to the top of the sqlite tree you are running, or sqlite.h may be found from
    some other installed location

  * from the directory:

    * $(SQLITE)/sqlite3
    * .load ./nfs4.so
    * .open file:172.31.24.76/db
    * select * from foo;
  
  * environment variables
     * NFS_PACKET_TRACE - show the byte contents of each request/response
     * NFS_TCP_NODELAY - set nodelay on the nfs socket
     * NFS_USE_FILEHANDLE - use cached filehandle instead of path for post-open operations
     * NFS_TRACE - additional logging information for NFS
     * NFS_READ_LIMIT - maximum size of rpc frame from server, default 1MB
     * NFS_WRITE_LIMIT - maximum size of rpc frame to server, default 1MB
     * NFS_OPS_LIMIT - maximum number of operations per rpc request, efs negotiates down to 16
     * NFS_REQUESTS_LIMIT - number of concurrent requests, default 32
