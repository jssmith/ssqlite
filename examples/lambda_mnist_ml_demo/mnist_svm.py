import numpy as np
import matplotlib.pyplot as plt
from sklearn import svm
from scipy import io
import time
from sklearn.svm import LinearSVC
from sklearn.metrics import accuracy_score
import json
from sklearn.model_selection import train_test_split


start = time.time()
# getting the data
with open('mnist_training_data.dat','rb') as f:
    mnist_training_data = np.load(f)

with open('mnist_training_labels.dat','rb') as f:
    mnist_training_labels = np.load(f)

with open('mnist_test_data.dat','rb') as f:
    mnist_test_data = np.load(f)


mnist_training_set_data, mnist_validation_set_data, mnist_training_set_labels , mnist_validation_set_labels  = train_test_split(mnist_training_data, mnist_training_labels, test_size=0.2, random_state=666)



# Initialize the hyperparameters numbers array and two scores array
hyperparameters = [10 ** e for e in range(-10,11)]
mnist_training_scores = np.array([])
mnist_validation_scores = np.array([])

for c in hyperparameters:
    # Initialize the linear svm model(C = c) --> train it with training set --> predict the labels of both sets
    mnist_svm = LinearSVC(C = c)
    mnist_svm.fit(mnist_training_set_data[:10000], mnist_training_set_labels[:10000])
    mnist_training_set_pred_labels = mnist_svm.predict(mnist_training_set_data)
    mnist_validation_set_pred_labels = mnist_svm.predict(mnist_validation_set_data)
    
    # Get the accuracy_score for both sets
    mnist_training_s = accuracy_score(mnist_training_set_labels, mnist_training_set_pred_labels)
    mnist_validation_s = accuracy_score(mnist_validation_set_labels, mnist_validation_set_pred_labels)
    
    # Append the score to arrays for plotting later
    mnist_training_scores = np.append(mnist_training_scores, mnist_training_s)
    mnist_validation_scores = np.append(mnist_validation_scores, mnist_validation_s)

 # Plotting error rate vs. training examples for both sets
hyperparameters = [str(c) for c in hyperparameters]
plt.figure(figsize=(20,5))
plt.plot(hyperparameters, mnist_training_scores, 'bs-', hyperparameters, mnist_validation_scores, 'g^-')
plt.xlabel('the hyperparameters')
plt.ylabel('error rate')
plt.title('MNIST error rate vs. hyperparameters')
plt.legend(('training_set', 'validation_set'))
plt.show()

end = time.time()
print(end - start)