# SFS Machine Learning Demo
This is the instruction of training a machine learning model using sfs in lambda

## Usage
1. Download python packages (numpy, sklearn, scipy, joblib) from PYPI, run 
   ```
   ./download_python_packages.sh
   ```
2. Put *.dat in efs
3. Zip all the files under /lambda_utils (joblib, numpy, sfs,lambda_function.py, scipy,sklearn) and upload the zip file to your lambda function

4. Run the main function
   ```
   python3 main.py
   ```
   Local version:
    ```
    python3 mnist_svm.py
    ```
