import ctypes

c_helper = ctypes.CDLL('./simpletest.so')

host_ip = '192.168.62.135'
file_name = 'testtxt'
b_host_ip = host_ip.encode('utf-8')
b_file_name = file_name.encode('utf-8')
c_helper.create_client_py.argtypes = [ctypes.c_char_p, ctypes.c_char_p]

c_helper.create_client_py(b_host_ip, b_file_name)
c_helper.create_file_py()
c_helper.read_file_py()