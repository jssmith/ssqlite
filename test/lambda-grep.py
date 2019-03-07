import json
import queue
import string
import threading
import time

import boto3

lambda_client = boto3.client('lambda')


def lambda_grep(file_path, phrase):
    """Call and return the result of an AWS Lambda call of grep on file_path.

    Returns:
        a dictionary of the Lambda response
        on success this will  include "matches",
        on error this will include "error_msg"
    """
    test_event = {
        "file_path": file_path,
        "phrase": phrase
    }

    invoke_response = lambda_client.invoke(
        FunctionName="SQLiteDemo-ssqlite-test",
        InvocationType='RequestResponse',
        Payload=json.dumps(test_event)
    )
    return json.loads(invoke_response['Payload'].read().decode("utf-8"))


def process_queue(q):
    """Continually print the result of Lambda grep calls."""
    while not q.empty():
        file_path, phrase = q.get()
        lambda_result = lambda_grep(file_path, phrase)
        lines = lambda_result.get('matches')
        if lines:
            print(''.join(lines))
        else:
            err_type = lambda_result.get('errorType')
            err_msg = lambda_result.get('errorMessage')
            if err_type and err_msg:
                print(f"{file_path}: {err_type} - {err_msg}\n")
        q.task_done()


def lambda_grep_multiple(file_paths, phrase, lambda_concurrency=4):
    """Print all lines in all files that the phrase occurs in.

    Args:
        file_paths: List of file paths to search.
        phrase: regex string to find matches of.
        lambda_concurrency: Max number of concurrent Lambda calls allowed.
    """
    # Create a Queue and fill with file paths to search
    q = queue.Queue()
    for file_path in file_paths:
        q.put((file_path, phrase))

    # Create threads_num threads to process the queue
    for i in range(lambda_concurrency):
        thread = threading.Thread(name="Consumer-"+str(i),
                                  target=process_queue,
                                  args=(q,))
        thread.start()

    # Wait for all files to be searched
    q.join()


if __name__ == "__main__":
    file_names = []
    for letter in string.ascii_lowercase:
        for digit in string.digits:
            file_name = "/docs/file-" + letter + digit + ".txt"
            file_names.append(file_name)

    file_paths = ["/aeneid_test.txt", "NotARealFile", "/aeneid_test2.txt"]
    phrase = "sn"
    start = time.time()
    lambda_grep_multiple(file_names[:100], phrase, 8)
    end = time.time()
    print(end - start)
