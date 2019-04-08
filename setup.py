import setuptools
from setuptools.command.install import install
from setuptools.command.test import test
from setuptools import Command
import subprocess
import os
import unittest
import traceback

BASEPATH = os.path.dirname(os.path.abspath(__file__))
NFSV4_PATH = os.path.join(BASEPATH, 'nfsv4')
SFS_PATH = os.path.join(BASEPATH, 'sfs')

class SfsInstall(install):
    def run(self):
        subprocess.call('make', cwd = NFSV4_PATH)
        install.run(self)

class SfsTest(test):
    description = "run sfs_test.py using unittest"
    user_options = [
        ('ip=', None, 'IP of NFS4_SERVER.'),
    ]
    def initialize_options(self):
        self.ip = None
    
    def finalize_options(self):
        pass
    
    def run(self):
        test_loader = unittest.TestLoader()
        test_suite = test_loader.discover('test', pattern='sfs_test.py')
        
        import sfs
        try:
            sfs.mount(self.ip)
        except Exception as e:
            print("================")
            print("ERROR:")
            print("NFS SERVER IP you entered: ", self.ip)
            if (self.ip == None):
                print("Please use --ip= option to set your NFS_SEVER IP")
            
            print("\nERROR DETAIL:")
            print(traceback.format_exc())
            print("================")


        unittest.TextTestRunner().run(test_suite)


class SfsClean(Command):
    """
    sfs custom command to clean out junk files.
    """
    description = "Cleans out junk files we don't want in the repo"
    user_options = []

    def initialize_options(self):
        pass

    def finalize_options(self):
        pass

    def run(self):
        subprocess.call(['make', 'clean'], cwd = NFSV4_PATH)
        subprocess.call(['rm', 'libnfs4.so'], cwd = SFS_PATH)
        os.system('rm -vrf ./build ./*.egg-info')
        
    
setuptools.setup(
    name="sfs",
    version="0.0.1",
    author="sfs team",
    author_email="sfs@example.com",
    description="A small example package",
    packages= setuptools.find_packages(),
    package_data={'sfs': ['libnfs4.so']},
    include_package_data=True,
    cmdclass={
        'install': SfsInstall,
        'test': SfsTest,
        'clean': SfsClean
    }
)
