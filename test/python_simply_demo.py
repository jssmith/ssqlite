import argparse
import binascii

from sfs import *

def clean_print(s):
    try:
        print(s.encode('utf-8'))
    except:
        print(binascii.hexlify(s))

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Demonstrate of SFS read with Python')
    parser.add_argument('--mount-point', type=str, required=True,
        help='Hostname or IP address of EFS mount point')
    parser.add_argument('--test-file', type=str, required=True,
        help='File name of test file to read')
    args = parser.parse_args()

    if not args.test_file.startswith('/'):
        raise ValueError('test-file should start with \'/\'')

    mount(args.mount_point)
    with open(args.test_file, mode='r',  buffering=256) as f:
        print("bytes [0, 128)")
        print(f.read(128))
        print("bytes [128, 256)")
        print(f.read(128))
        f.seek(0)
        print("bytes [0, 64)")
        print(f.read(64))
