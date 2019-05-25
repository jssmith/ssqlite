import json
from multiprocessing import Pool
import matplotlib.pyplot as plt
import boto3
import time

start = time.time()

lambda_client = boto3.client('lambda')

def lambda_function(c):
    input_event = {"hyperparameter": c}

    invoke_response = lambda_client.invoke(
                FunctionName="test",
                InvocationType='RequestResponse',
                Payload=json.dumps(input_event)
            )
    
    response = json.loads(invoke_response['Payload'].read().decode("utf-8"))

    return (c, float(response['train_score']), float(response['valid_score']), float(response['elapsed_time']))


p = Pool(21)
hyperparameters = [10 ** e for e in range(-10,11)]
result = p.map(lambda_function, hyperparameters)
print('Response:')
print(result)
print()

end = time.time()
print('Elapsed Time:')
print(end - start)

hyperparameters = [ str(item[0]) for item in result]
mnist_training_scores = [ item[1] for item in result]
mnist_validation_scores = [ item[2] for item in result]



plt.figure(figsize=(20,5))
plt.plot(hyperparameters, mnist_training_scores, 'bs-', hyperparameters, mnist_validation_scores, 'g^-')
plt.xlabel('the hyperparameters')
plt.ylabel('error rate')
plt.title('MNIST error rate vs. hyperparameters')
plt.legend(('training_set', 'validation_set'))
plt.show()