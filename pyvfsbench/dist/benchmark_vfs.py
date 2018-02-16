import json
import os
import socket
import subprocess

import vfsbench


log_file = "/tmp/perf_log.json"

def ensure_test_file(fn):
    if not os.path.isfile(fn):
        print("creating test file")
        if subprocess.call(["dd", "if=/dev/urandom", "of=%s" % fn, "bs=4096", "count=10000"]):
            print("error creating test file")
        else:
            print("success")
    else:
        print("test file already exists")

def clear_results():
    if os.path.isfile(log_file):
        os.unlink(log_file)

def load_results():
    with open(log_file, "r") as f:
        data = f.read()
    return data

def lambda_handler(event, context):
    block_size = int(event["blockSize"]) if "blockSize" in event else 4096
    num_operations = int(event["numOperations"]) if "numOperations" in event else 100
    num_threads = int(event["numThreads"]) if "numThreads" in event else 1
    file_name = event["fileName"] if "fileName" in event else "/testfile"
    mode = event["mode"] if "mode" in event else "unix"
    if mode == "unix":
        test_file = "/tmp%s" % file_name
        ensure_test_file(test_file)

        clear_results()
        res = vfsbench.benchmark_vfs("unix",test_file,block_size,num_operations,num_threads,"rr")
        res = load_results()
    elif mode == "nfs":
        clear_results()
        efs_location = "%s.efs.%s.amazonaws.com" % (os.environ["EFS_HOSTNAME"], os.environ["AWS_REGION"])
        test_location = "%s%s" % (socket.gethostbyname(efs_location), file_name)
        print("location %s resolves to %s" % (efs_location, test_location))
        res = vfsbench.benchmark_vfs("nfs4",test_location,block_size,num_operations,num_threads,"rr")
        res = load_results()

    return json.loads(res)
