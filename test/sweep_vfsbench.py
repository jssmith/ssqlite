import json
import socket
import subprocess

# dd if=/dev/urandom of=/tmp/test_1gb bs=1M count=1024
# dd if=/dev/urandom of=/efs/test_1gb bs=1M count=1024

experiments = [
    {
        "experiment_spec" : {
            "storage_location": "ebs",
            "size": "1gb"
        },
        "vfs_name": "unix",
        "data_path": "/tmp/test_1gb"
    },
    {
        "experiment_spec" : {
            "storage_location": "efs",
            "size": "1gb"
        },
        "vfs_name": "unix",
        "data_path": "/efs/test_1gb"
    },
    {
        "experiment_spec" : {
            "storage_location": "nfs",
            "size": "1gb"
        },
        "vfs_name": "unix",
        "data_path": "/nfs/test_1gb"
    },
    {
        "experiment_spec" : {
            "storage_location": "ebs-user",
            "size": "1gb"
        },
        "vfs_name": "nfs4",
        "data_path": "%s/test_1gb" % socket.gethostbyname("fs-ae2724e7.efs.us-east-1.amazonaws.com")
    },
    {
        "experiment_spec" : {
            "storage_location": "nfs-user",
            "size": "1gb"
        },
        "vfs_name": "nfs4",
        "data_path": "192.168.1.15/test_1gb"
    },
    # {
    #     "experiment_spec" : {
    #         "storage_location" = "ramdisk",
    #         "size" = "1gb"
    #     },
    #     "vfs": "unix"
    #     "data_path": "/ramdisk/test_1gb"
    # }
]

# block_sizes = [ 256, 512, 1024, 2048, 4096, 8192, 16384, 32768, 65536 ]
# num_threads = [ 1, 2, 4, 8, 16 ]

#block_sizes = [ 1024, 2048, 4096, 8192 ]
block_sizes = [4096]
num_threads = [ 1 ]
#modes = [ "rr", "rw", "sr", "sw" ]
modes = ["rw", "sw"]

def exec_vfsbench(experiment_spec, vfs_name, data_path, block_size, blocks_per_thread, num_threads, mode):
    args = ["./vfsbench", experiment_spec, vfs_name, data_path, str(int(block_size)),
        str(int(blocks_per_thread)), str(int(num_threads)), mode]
    print("running with", args)
    res = subprocess.call(args)

def flush_caches():
    res = subprocess.call(["/bin/sync"])
    if res:
        print("problem in sync")
        sys.exit(1)
    res = subprocess.call(["/usr/bin/sudo", "/bin/bash", "-c", "echo 1 > /proc/sys/vm/drop_caches"])
    if res:
        print("problem in flush caches")
        sys.exit(1)



if __name__ == "__main__":
    scale = 10 * 1048576
    for bs in block_sizes:
        for mode in modes:
            blocks_per_thread = scale / bs
            for nt in num_threads:
                for e in experiments:
                    flush_caches()
                    exec_vfsbench(json.dumps(e["experiment_spec"]), e["vfs_name"], e["data_path"], bs, blocks_per_thread, nt, mode)
