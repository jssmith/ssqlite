import os
import sfs
import logging

logging.basicConfig(
        level=os.environ.get("LOGLEVEL", "DEBUG"),
        format="%(asctime)s\t%(levelname)s\t%(funcName)s\t\tt%(message)s")


mount_point = os.environ["NFS4_SERVER"]
sfs.mount(mount_point)

f = sfs.open("/cat.jpg", "rb", buffering=-1)
for i in range(6,7):
    logging.debug("new read of size %s", 10**i)
    f.seek(0)
    bytes_read = f.read(10**i)
    logging.debug("read %s bytes", bytes_read)
f.close()

f = sfs.open("basic_write.md", "w")
f.write("okay")
