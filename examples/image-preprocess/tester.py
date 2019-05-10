import sfs
import logging
import os
import time

logging.basicConfig(
        level=os.environ.get("LOGLEVEL", "DEBUG"),
        format="%(asctime)s\t%(levelname)s\t%(funcName)s\t\t%(message)s")

mount_point = os.environ["NFS4_SERVER"]
sfs.mount(mount_point)

f = sfs.open("/cat.jpg", "rb")
f.read()
print(f.tell())
