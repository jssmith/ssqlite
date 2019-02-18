# Shell
A client to interact with the NFS4 protocol

## Commands

Each command takes zero or more arguments and return a value. Values can be a PATH, FILE, ERROR, or BUFFER. Each command is evaluated with the rest of the line as arguments. Thus the last command is evaluated first.

For example, `write open /sample.txt generate 2` can be thought of as `write(open(/sample.txt), generate(2))`.

`create` - creates an empty file in reference to the current directory
`write` - create a FILE at the given PATH with given BUFFER `write open /sample.txt generate 2`
`append`
`awrite`
`read`
`md5`
`rm` - removes the file
`generate` - return a BUFFER with randomly generated content - `generate 2`
`ls` - displays the contents of the current directory
`cd` - not implemented
`open` 
`chmod`
`chown`
`conn`
`local`
`config`
`compare`
`lock`
`truncate`
`unlock`
`mkdir` - make a directory 

## Environment Flags
`NFS4_SERVER` - the address of the NFS server to connect to
`NFS_TRACE`
`NFS_USE_ROOTFH`
`NFS_USE_PUTROOTFH`
`NFS_IGNORE_SIGPIPE`
`NFS_AUTH_NULL`
`NFS_PACKET_TRACE`
`NFS_TCP_NODELAY`
