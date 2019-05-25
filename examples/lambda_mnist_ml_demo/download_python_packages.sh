# Download numpy, sklearn, scipy, joblib from pypi
wget https://files.pythonhosted.org/packages/bb/76/24e9f32c78e6f6fb26cf2596b428f393bf015b63459468119f282f70a7fd/numpy-1.16.3-cp37-cp37m-manylinux1_x86_64.whl
wget https://files.pythonhosted.org/packages/21/a4/a48bd4b0d15395362b561df7e7247de87291105eb736a3b2aaffebf437b9/scikit_learn-0.21.2-cp37-cp37m-manylinux1_x86_64.whl
wget https://files.pythonhosted.org/packages/5d/bd/c0feba81fb60e231cf40fc8a322ed5873c90ef7711795508692b1481a4ae/scipy-1.3.0-cp37-cp37m-manylinux1_x86_64.whl
wget https://files.pythonhosted.org/packages/cd/c1/50a758e8247561e58cb87305b1e90b171b8c767b15b12a1734001f41d356/joblib-0.13.2-py2.py3-none-any.whl

# unpack packages
for package in *.whl
do
    unzip $package -d lambda_utils && rm $package
done


for t in lambda_utils/*.dist-info
do
    rm -rf $t
done