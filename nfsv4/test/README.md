# Shell
A client to interact with the NFS4 protocol

## Example Shell Usage

After ensuring all necessary environment variables are set, run `shell`. 
We've mounted NFS at `/efs/` and stored `aeneid.txt` at `/efs/aeneid.txt`.   

Opening a file

    > open /aeneid.txt
    0.002389004

Reading a file. 
1024 is the length of `aeneid.txt`.

    > read open /aeneid.txt 
    [1024]
    0.003774003

Writing a file with two bytes of random content.

    > write open /aeneid.txt generate 2
    0.027

## Commands

Each command takes zero or more arguments and return a value. Values can be a PATH, FILE, ERROR, INTEGER, or BUFFER. Some of the commands have been labeled with their argument types

Each command is evaluated with the rest of the line as arguments. Thus the last command is evaluated first.

For example, `write open /sample.txt generate 2` can be thought of as `write(open(/sample.txt), generate(2))`.

`create PATH` - create and return an empty FILE in reference to the current directory
`write FILE BUFFER` - create and return a FILE at the given PATH with given BUFFER 
`append`
`awrite`
`read FILE` - return a BUFFER of the contents of FILE.
`md5`
`rm PATH` - removes the file at PATH
`generate INTEGER` - return a BUFFER with INTEGER bytes of randomly generated content 
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

- `NFS4_SERVER` - the address of the NFS server to connect to
- `NFS_TRACE`
- `NFS_USE_ROOTFH`
- `NFS_USE_PUTROOTFH`
- `NFS_IGNORE_SIGPIPE`
- `NFS_AUTH_NULL`
- `NFS_PACKET_TRACE`
- `NFS_TCP_NODELAY`
