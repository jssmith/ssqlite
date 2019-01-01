import sfs
import unittest
import string
import random
import os
import io
import sys

class TestOpen(unittest.TestCase):
    def test_read_mode(self):
        #'r' open for reading (default)
        # can read, unable to write, error on file does not exist, 
        # error on missing read permission (including directory tree)
        '''
        # 1. generate a 15-character-long string as filename, chance that it exists is close to zero
        filename = '/'+''.join(random.choices(string.ascii_uppercase + string.digits, k=15))
        self.assertRaises(FileNotFoundError, sfs.open, filename, "r") # error on non-exist file
        #f = sfs.open(filename, "r")
        
        # 2. check if can read, make you have file 'haha.txt' in your /efs/
        f = sfs.open('haha.txt', 'r')
        self.assertEqual(f.read(20), "this is a test file")

        # 3. check if cannot write
        self.assertRaises(io.UnsupportedOperation, f.write, "1")
        '''
        f = sfs.open("1", "r")

if __name__ == '__main__':
    #parser = argparse.ArgumentParser(description='Testing of SFS open with Python')
    #parser.add_argument('--mount-point', type=str, required=True,
    #   help='Hostname or IP address of EFS mount point')
    #args = parser.parse_args()
    #sfs.mount(args.mount_point)
    sfs.mount(sys.argv.pop())
    unittest.main()
