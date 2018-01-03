import os
import random
import socket
import sys
import sqlite3

import tpcc
import logging
from util import *
from tpccrt import *

lambda_id = "%016x" % random.getrandbits(64)
efs_location = "%s.efs.%s.amazonaws.com" % (os.environ["EFS_IP"], os.environ["AWS_REGION"])
database_location = "%s/tpcc-nfs" % socket.gethostbyname(efs_location)

is_ext_loaded = False

def load_nfs4_extension():
    global is_ext_loaded
    if not is_ext_loaded:
        init_conn = sqlite3.connect(":memory:")
        init_conn.enable_load_extension(True)
        init_conn.load_extension("./nfs4.so")
        init_conn.close()
        is_ext_loaded = True

def test_local():
    conn = sqlite3.connect("/tmp/test-db")
    conn.execute('CREATE TABLE IF NOT EXISTS test_write_table (id INT)')
    conn.execute('INSERT INTO test_write_table (id) VALUES (5)')
    c = conn.cursor()
    c.execute('SELECT COUNT(*) FROM test_write_table')
    num_rows = c.fetchone()[0]
    print("number of rows in test table", num_rows)
    conn.commit()
    conn.close()
    return { "NumRows": num_rows }

def test_open():
    load_nfs4_extension()

    conn = sqlite3.connect(database_location)
    c = conn.cursor()
    c.execute('SELECT COUNT(*) FROM warehouse')
    num_warehouses = c.fetchone()[0]
    print("number of warehouses", num_warehouses)
    conn.close()
    return { "NumWarehouses" : num_warehouses }

def test_write():
    load_nfs4_extension()

    conn = sqlite3.connect(database_location)
    conn.execute('CREATE TABLE IF NOT EXISTS test_write_table (id INT)')
    conn.execute('INSERT INTO test_write_table (id) VALUES (5)')
    c = conn.cursor()
    c.execute('SELECT COUNT(*) FROM test_write_table')
    num_rows = c.fetchone()[0]
    print("number of rows in test table", num_rows)
    conn.commit()
    conn.close()
    return { "NumRows": num_rows }

def test_create():
    load_nfs4_extension()

    create_database_location = "%s/test-%s" % (socket.gethostbyname(efs_location), lambda_id)
    conn = sqlite3.connect(create_database_location)
    conn.execute('CREATE TABLE IF NOT EXISTS test_write_table (id INT)')
    conn.execute('INSERT INTO test_write_table (id) VALUES (5)')
    c = conn.cursor()
    c.execute('SELECT COUNT(*) FROM test_write_table')
    num_rows = c.fetchone()[0]
    print("number of rows in test table", num_rows)
    conn.commit()
    conn.close()
    return { "NumRows": num_rows }

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
    config["reset"] = False
    config["load"] = False
    config["execute"] = False
    driver.loadConfig(config)
    tpcc.logging.info("Initializing TPC-C benchmark using %s" % driver)

    ## Create ScaleParameters
    scaleParameters = scaleparameters.makeWithScaleFactor(args["warehouses"], args["scalefactor"])
    nurandx = rand.setNURand(nurand.makeForLoad())

    e = executor.Executor(driver, scaleParameters, stop_on_error=args["stop_on_error"])
    driver.executeStart()
    results = e.execute(args["duration"])
    driver.executeFinish()
    return results.show()


def lambda_handler(event, context):
    print("lambda id", lambda_id)
    print("database location", database_location)
    tests = {
        "local": test_local,
        "open": test_open,
        "write": test_write,
        "create": test_create,
        "tpcc": do_tpcc }
    res = tests[event["test"]]()
    return {
        "LambdaId": lambda_id,
        "AwsRequestId": context.aws_request_id,
        "Result": res }
