# Shell
A client to interact with the NFS4 protocol

## Example Shell Usage

After ensuring all necessary environment variables are set, run `shell`. 
We've mounted NFS at `/efs/` and stored `aeneid.txt` at `/efs/aeneid.txt`.   

Opening a file

    > open /aeneid.txt
    0.002389004

Reading a file. 1024 is the length of `aeneid.txt`.

    > read open /aeneid.txt 
    [1024]
    0.003774003

Writing a file currently segfaults.

    > write open /aeneid.txt
    [SEGFAULT]

## Commands

- `create` - creates an empty file in reference to the current directory
- `write`
- `append`
- `awrite`
- `read`
- `md5`
- `rm` - removes the file
- `generate`
- `ls` - displays the contents of the current directory
- `cd` - not implemented
- `open` 
- `chmod`
- `chown`
- `conn`
- `local`
- `config`
- `compare`
- `lock`
- `truncate`
- `unlock`
- `mkdir` - make a directory 

## Environment Flags

- `NFS4_SERVER` - the address of the NFS server to connect to
- `NFS_TRACE`
- `NFS_USE_ROOTFH`
- `NFS_USE_PUTROOTFH`
- `NFS_IGNORE_SIGPIPE`
- `NFS_AUTH_NULL`
- `NFS_PACKET_TRACE`
- `NFS_TCP_NODELAY`
