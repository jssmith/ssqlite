"""
Tests the reading of binary files in SFS.

Compares the read bytes from a file that has been opened with SFS
with the read bytes from a file that has been opened through regular open.

Expects environment variables NFS4_SERVER to contain the NFS server IP.
"""

import os
import sfs
import logging
import io

NFS_PATH = "/cat.jpg"
REG_PATH = "/efs/cat.jpg"

mount_point = os.environ["NFS4_SERVER"]
sfs.mount(mount_point)

nfs_f = io.BytesIO(sfs.open(NFS_PATh, "rb").read())
normal_f = open(REG_PATH, "rb")

nfs_read = nfs_f.read()
normal_read = normal_f.read()

print("content head")
print(nfs_read[:100])

if len(nfs_read) != len(normal_read):
    print(len(nfs_read))
    print(len(normal_read))

for i, b in enumerate(nfs_read):
    if b != normal_read[i]:
        print("Contents differ")
        print(b)
        print(normal_read[i])
        break
