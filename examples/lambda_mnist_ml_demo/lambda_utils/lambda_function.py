import json
import sfs
import numpy as np
import io
from sklearn.model_selection import train_test_split
from sklearn.svm import LinearSVC
from sklearn.metrics import accuracy_score
import time


def lambda_handler(event, context):
    start = time.time()
    sfs.mount('192.168.62.135')

    # Reading data using sfs
    with sfs.open('/mnist_training_data.dat', 'rb', buffering=2**20) as f:
        mnist_training_data = np.load(io.BytesIO(f.read()))

    with sfs.open('/mnist_training_labels.dat', 'rb', buffering=2**20) as f:
        mnist_training_labels = np.load(io.BytesIO(f.read()))

    X_train, X_valid, y_train, y_valid = train_test_split(mnist_training_data, mnist_training_labels, test_size=0.2, random_state=666)

    # Training the model (Linear SVM)
    c = event['hyperparameter']
    mnist_svm = LinearSVC(C = c)
    mnist_svm.fit(X_train[:10000], y_train[:10000])
    pred_y_train = mnist_svm.predict(X_train)
    pred_y_valid = mnist_svm.predict(X_valid)
    
    # Get the accuracy_score for both sets
    score_train = accuracy_score(pred_y_train, y_train)
    score_valid = accuracy_score(pred_y_valid, y_valid)

    end = time.time()

    return {
        'statusCode': 200,
        'train_score': json.dumps(score_train),
        'valid_score': json.dumps(score_valid),
        'elapsed_time': end - start
    }
