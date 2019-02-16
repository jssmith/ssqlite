import sfs
import unittest
import string
import random
import os
import io
import sys

class TestOpen(unittest.TestCase):
    def test_read_mode(self):
        ''' 
        'r': open for reading text, synonym of 'rt' (default).
        Demonstrate that can read, unable to write, error on file does not exist, 
        error on missing read permission (including directory tree) 
        '''
        
        # 1. check if error on a non-existing file
        # generate a 15-character-long letters-digits-mixed string as filename, 
        # chance that it already exists is close to zero.
        # Open a file with such filename should raise a FileNotFoundError exception
        random.seed(6) # I like 6
        filename = '/'+''.join(random.choices(string.ascii_letters + string.digits, k=15))
        self.assertRaises(FileNotFoundError, sfs.open, filename, "r") 
        
        # 2. check if can read an existing file
        new_f = open("/efs/test.txt", 'w+') # make sure to run this script as ROOT
        content = "This is a test file."
        new_f.write(content)
        new_f.close()
        f = sfs.open('/test.txt', 'r')
        self.assertEqual(f.read(len(content)).decode('utf-8'), content)

        # 3. check if cannot write
        self.assertRaises(io.UnsupportedOperation, f.write, "1")
       
        # 4. check if error on missing read permission
        # TODO: currently sfs.open DOES NOT raise an exception
        new_f = open("/efs/no_read_perm.txt", 'w+')
        new_f.close()
        os.chmod("/efs/no_read_perm.txt", 0o000)
        self.assertRaises(PermissionError, sfs.open, "/no_read_perm.txt", 'r')
    
    def test_write_mode(self):
        '''
        'w': open for writing, truncting the file first. Create if the file doesn't exist.
        Demonstrate that unable to read, can write to and truncate an existing file, 
        can create a file if it doesn't exist, error on an existing file that doesn't have write permission,
        error on missing directory permission (including on the directory tree) 
        '''
        # 1. check if opening a non-existing file will first create a such file
        # generate a 15-character-long letters-digits-mixed string as filename, 
        # chance that it already exists is close to zero.
        random.seed(8) # I also like 8
        filename = '/'+''.join(random.choices(string.ascii_letters + string.digits, k=15))
        f = sfs.open(filename, 'w')
        self.assertTrue(os.path.isfile('/efs'+filename))

        # 2. check if write-only TODO: write-only yet to be supported
        # new_f = open('/efs/test.txt', 'w+')
        # new_f.write('You cannot read it')
        # new_f.close()
        # f = sfs.open('/test.txt', 'w')
        # self.assertRaises(io.UnsupportedOperation, f.read, 1)

        # 3. write to an empty file.
        # first create an empty file using Python built-in open
        filename = '/'+''.join(random.choices(string.ascii_letters + string.digits, k=15))
        new_f = open('/efs'+filename, 'w+') 
        new_f.close()
        # open and write using sfs library
        content = 'Writing to an empty file'
        f = sfs.open(filename, 'w')
        n = f.write(content)
        self.assertEqual(n, len(content)) # make sure sfs.write returns the correct number of bytes written
        # use Python built-in open and read to check the content written by sfs.write
        new_f = open('/efs' + filename, 'r')
        self.assertEqual(new_f.read(len(content)), content) # check written content
        new_f.close()

        # 4. write to a non-empty existing file, should truncate the file first
        filename = '/'+''.join(random.choices(string.ascii_letters + string.digits, k=15))
        new_f = open('/efs'+filename, 'w+')
        old_content = 'This is a test file'
        n = new_f.write(old_content)
        new_f.close()
        f = sfs.open(filename, 'w')
        new_content = 'Writing to a non-empty file'
        n = f.write(new_content)
        self.assertEqual(n, len(new_content))
        new_f = open('/efs'+filename, 'r')
        self.assertEqual(new_f.read(len(new_content)), new_content)
        new_f.close()

    def test_append_mode(self):
        '''
        'a': open for writing, appending to the end of file if it exists. Create the file if it doesn't exist.
        Demonstrate that unable to read, can append to the end of an existing file, 
        can create a file if it doesn't exist, error on an existing file that doesn't have write permission,
        error on missing directory permission (including on the directory tree) 
        '''
        # 1. check if opening a non-existing file will first create a such file
        # generate a 15-character-long letters-digits-mixed string as filename, 
        # chance that it already exists is close to zero.
        random.seed(68) # I like 68
        filename = '/'+''.join(random.choices(string.ascii_letters + string.digits, k=15))
        f = sfs.open(filename, 'a')
        self.assertTrue(os.path.isfile('/efs'+filename))

        # 2. check if write-only TODO: write-only yet to be supported
        new_f = open('/efs/test.txt', 'w+')
        new_f.write('You cannot read it')
        new_f.close()
        f = sfs.open('/test.txt', 'a')
        self.assertRaises(io.UnsupportedOperation, f.read, 1)

        # 3. write to an empty file.
        # first create an empty file using Python built-in open
        filename = '/'+''.join(random.choices(string.ascii_letters + string.digits, k=15))
        new_f = open('/efs'+filename, 'w+') 
        new_f.close()
        # open and write using sfs library
        content = 'Writing to an empty file'
        f = sfs.open(filename, 'a')
        n = f.write(content)
        self.assertEqual(n, len(content)) # make sure sfs.write returns the correct number of bytes written
        # use Python built-in open and read to check the content written by sfs.write
        new_f = open('/efs' + filename, 'r')
        self.assertEqual(new_f.read(len(content)), content) # check written content
        new_f.close()

        # 4. write to a non-empty existing file, should truncate the file first
        filename = '/'+''.join(random.choices(string.ascii_letters + string.digits, k=15))
        new_f = open('/efs'+filename, 'w+')
        old_content = 'This is a test file'
        n = new_f.write(old_content)
        new_f.close()
        f = sfs.open(filename, 'a')
        new_content = 'Writing to a non-empty file'
        n = f.write(new_content)
        self.assertEqual(n, len(new_content))
        new_f = open('/efs'+filename, 'r')
        self.assertEqual(new_f.read(len(old_content)+len(new_content)), old_content+new_content)
        new_f.close()

if __name__ == '__main__':
    """
    run all tests by "sudo python3 sfs_test.py ${NFS4_SERVER}"
    """
    sfs.mount(sys.argv.pop())
    unittest.main()
