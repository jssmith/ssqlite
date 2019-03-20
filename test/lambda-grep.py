import json
import multiprocessing as mp
import os
import string
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
    start = time.time()
    test_event = {
        "file_path": file_path,
        "phrase": phrase
    }

    invoke_response = lambda_client.invoke(
        FunctionName="SQLiteDemo-ssqlite-test",
        InvocationType='RequestResponse',
        Payload=json.dumps(test_event)
    )
    end = time.time()
    print(os.getpid(), "is searching", file_path, "in", end - start)
    return json.loads(invoke_response['Payload'].read().decode("utf-8"))


def lambda_grep_multiple(file_paths, phrase, lambda_concurrency=4):
    """Print all lines in all files that the phrase occurs in.

    Args:
        file_paths: List of file paths to search.
        phrase: regex string to find matches of.
        lambda_concurrency: Max number of concurrent Lambda calls allowed.
    """

    pool = mp.Pool(processes=lambda_concurrency)
    results = pool.starmap(lambda_grep, zip(
        file_paths, [phrase for file_path in file_paths]))
    pool.close()
    pool.join()
    return results


def generate_file_names():
    file_names = []
    for letter in string.ascii_lowercase:
        for digit in string.digits:
            file_name = "/docs/file-" + letter + digit + ".txt"
            file_names.append(file_name)
    return file_names


if __name__ == "__main__":
    file_names = generate_file_names()
    file_paths = ["/aeneid_test.txt", "NotARealFile", "/aeneid_test2.txt"]
    phrase = "awe"

    start = time.time()
    print(lambda_grep_multiple(file_names[:100], phrase, 8))
    end = time.time()
    print(end - start)
