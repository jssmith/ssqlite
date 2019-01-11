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
        
        # 1. generate a 15-character-long string as filename, chance that it exists is close to zero.
        # Open a file with such filename should raise a FileNotFoundError exception
        filename = '/'+''.join(random.choices(string.ascii_letters + string.digits, k=15))
        self.assertRaises(FileNotFoundError, sfs.open, filename, "r") # error on non-exist file
        
        # 2. check if can read an existing file
        new_f = open("/efs/test.txt", 'w+') # make sure to run this script as ROOT
        content = "this is a test file"
        new_f.write(content)
        new_f.close()
        f = sfs.open('/test.txt', 'r')
        self.assertEqual(f.read(len(content)).decode('utf-8'), content)

        # 3. check if cannot write
        self.assertRaises(io.UnsupportedOperation, f.write, "1")
       
        # 4. check if missing read permission
        # CURRENTLY NOT WORKING
        new_f = open("/efs/no_read_perm.txt", 'w+')
        new_f.close()
        os.chmod("/efs/no_read_perm.txt", 0o000)
        self.assertRaises(PermissionError, sfs.open, "/no_read_perm.txt", 'r')
        

if __name__ == '__main__':
    """
    run this test by "sudo python3 sfs_test.py ${NFS4_SERVER}"
    """
    sfs.mount(sys.argv.pop())
    unittest.main()
