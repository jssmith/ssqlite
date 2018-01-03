import json
import subprocess
import sys
import re

# The paramsweep script runs the TPC-C benchmark using a variety of
# configuration parameters.
#
# Before running this script you need to create an initial database in a
# temporary location.
#
#     LD_PRELOAD=$PATH_TO_SQLITE_BUILD/.libs/libsqlite3.so \
#         python3 tpcc.py \
#         --config initializaion-config \
#         --no-execute sqlite
#
# Experiments will all begin by copying the same initial database to a test
# location.

def init_location(location, vfs):

    def cp(location):
        res = subprocess.call(["/bin/cp", "/tmp/tpcc-initial", location])
        if res:
            print("problem in copy")
            sys.exit(1)

    def su_cp(location):
        res = subprocess.call(["/usr/bin/sudo", "-u", "nfsnobody", "-g", "nfsnobody", "/bin/cp", "/tmp/tpcc-initial", location])
        if res:
            print("problem in copy")
            sys.exit(1)

    if vfs == "nfs4":
        p = re.compile("^[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+/(.*)$")
        m = p.match(location)
        if not m:
            print("failure to match on location", location)
            sys.exit(1)
        mount_location = "/efs/%s" % m.group(1)
        print("translated", location, mount_location)
        su_cp(mount_location)
        res = subprocess.call(["/usr/bin/sudo", "/bin/chown", "nfsnobody.nfsnobody", mount_location])
        if res:
            print("problem in chown")
            sys.exit(1)
    else:
        cp(location)

    res = subprocess.call(["/bin/sync"])
    if res:
        print("problem in sync")
        sys.exit(1)
    res = subprocess.call(["/usr/bin/sudo", "/bin/bash", "-c", "echo 1 > /proc/sys/vm/drop_caches"])
    if res:
        print("problem in flush caches")
        sys.exit(1)

def run_test(config_file):
    env = { "LD_PRELOAD": "/home/ec2-user/sqlite-build/.libs/libsqlite3.so" }
    args = ["python", "tpcc.py", "--config", config_file, "--no-load", "sqlite" ]
    p = subprocess.Popen(args, env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    res = p.communicate()
    print(res)
    return res

def save_results(filename, results):
    with open(filename, "w") as outfile:
        json.dump(results, outfile)

if __name__ == "__main__":
    databases = [
        { "path": "/ramdisk/tpcc", "vfs": "unix" },
        { "path": "/tmp/tpcc", "vfs": "unix" },
        { "path": "/efs/tpcc", "vfs": "unix" },
        { "path": "/nfs/tpcc", "vfs": "unix"},
        { "path": "192.168.1.57/tpcc", "vfs": "nfs4" } ]
    locking_modes = [ "normal", "exclusive" ]
    journal_modes = [ "delete", "wal" ]
    cache_sizes = [ 2000, 20000 ]

    results = []
    for database in databases:
        for locking_mode in locking_modes:
            for journal_mode in journal_modes:
                for cache_size in cache_sizes:
                    config = {
                        "database": database["path"],
                        "vfs": database["vfs"],
                        "journal_mode": journal_mode,
                        "locking_mode": locking_mode,
                        "cache_size": cache_size }
                    with open("tmp-config", "w") as f:
                        f.write("# Auto-generated SQLite configuration file\n")
                        f.write("[sqlite]\n\n")
                        for k, v in config.items():
                            f.write("%s = %s\n" % (k, str(v)))
                    print("executing ", config)
                    init_location(database["path"], database["vfs"])
                    res = run_test("tmp-config")
                    results.append({ "config" : config, "results": [res[0][:10000],res[1][:10000]] })

                    # save out the results on every iteration in case something breaks
                    # xxx maybe better to append one json object per line
                    save_results("results.txt", results)
