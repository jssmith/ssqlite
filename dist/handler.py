import os
import socket
import sys
import sqlite3


import tpcc
import logging
from util import *
from tpccrt import *

efs_location = "%s.efs.%s.amazonaws.com" % (os.environ["EFS_IP"], os.environ["AWS_REGION"])
database_location = "%s/tpcc-nfs" % socket.gethostbyname(efs_location)

def test_open():
    init_conn = sqlite3.connect(":memory:")
    init_conn.enable_load_extension(True)
    init_conn.load_extension("./nfs4.so")
    init_conn.close()

    conn = sqlite3.connect(database_location)
    c = conn.cursor()
    c.execute('SELECT count(*) FROM warehouse')
    print("number of warehouses", c.fetchone()[0])
    conn.close()

def do_tpcc():
    # print(os.environ["NFS_TRACE"])
    system = "sqlite"
    args = {
        "warehouses": 4,
        "scalefactor": 1,
        "duration": 10,
        "stop_on_error": True,
        "debug": True
    }
    if args['debug']: logging.getLogger().setLevel(logging.DEBUG)
    driverClass = tpcc.createDriverClass(system)
    assert driverClass != None, "Failed to find '%s' class" % system
    driver = driverClass("tpcc.sql")

    ## Load Configuration file
    config = {
        "database": database_location,
        "vfs": "nfs4",
        "journal_mode": "delete",
        "locking_mode": "exclusive",
        "cache_size": 2000
    }
    config['reset'] = False
    config['load'] = False
    config['execute'] = False
    driver.loadConfig(config)
    tpcc.logging.info("Initializing TPC-C benchmark using %s" % driver)

    ## Create ScaleParameters
    scaleParameters = scaleparameters.makeWithScaleFactor(args['warehouses'], args['scalefactor'])
    nurandx = rand.setNURand(nurand.makeForLoad())

    e = executor.Executor(driver, scaleParameters, stop_on_error=args['stop_on_error'])
    driver.executeStart()
    results = e.execute(args['duration'])
    driver.executeFinish()
    return results


def lambda_handler(event, context):
    print("database location", database_location)
    test_open()
    #print(do_tpcc().show())
    return "done"
