import os
import sqlite3
import sys
from subprocess import PIPE, STDOUT, Popen

print('Loading function')


def ls():
    p = Popen("ls /usr/lib64/",shell=True,stdout=PIPE, stderr=STDOUT)
    for line in p.stdout.readlines():
        print(line)
    retval = p.wait()

def test_sqlite(conn):
    c = conn.cursor()

    c.execute('''CREATE TABLE IF NOT EXISTS stocks
                 (date text, trans text, symbol text, qty real, price real)''')

    c.execute("INSERT INTO stocks VALUES ('2006-01-05','BUY','RHAT',100,35.14)")

    t = ('RHAT',)
    c.execute('SELECT * FROM stocks WHERE symbol=?', t)
    print(c.fetchone())

    c.execute('SELECT count(*) FROM stocks WHERE symbol=?', t)
    ct = c.fetchone()[0]
    conn.commit()

    return ct

def test_nfs():
    nfs_url = "nfs://%s.efs.%s.amazonaws.com:2049/test.txt" % (os.environ["EFS_IP"], os.environ["AWS_DEFAULT_REGION"])
    print("nfs url: %s" % nfs_url)
    p = Popen(['/var/task/nfs4-cat', nfs_url],stdout=PIPE, stderr=STDOUT)
    for line in p.stdout.readlines():
        print(line)
    retval = p.wait()
    return retval

def lambda_handler(event, context):
    print(os.environ)
    print(sys.version_info)
    conn = sqlite3.connect('/tmp/example.db')
    conn.enable_load_extension(True)
    # conn.load_extension('vfsstat')
    print("sqlite version", sqlite3.sqlite_version)
    ct = test_sqlite(conn)
    conn.close()

    nfs_retval = test_nfs()

    return "DONE - %s rows found, NFS test returned %s" % (ct, nfs_retval)
