"""Distributed multifile grep for usage on AWS Lambda and local files."""

import argparse
import json
import multiprocessing as mp
import os
import string
import time

import boto3
import grep_handler

lambda_client = boto3.client('lambda')


def lambda_grep_single(file_path, phrase, _):
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


def local_grep_single(file_path, phrase, local):
    """Call and return the result of local grep on file_path.

    Returns:
        a dictionary of the Lambda response
        on success this will  include "matches",
        on error this will include "error_msg"
    """
    start = time.time()
    matches = grep_handler.grep_single(file_path, phrase, local)
    end = time.time()

    print(os.getpid(), "is searching", file_path, "in", end - start)
    return {'matches': matches, 'file_name': file_path}


def lambda_grep_multiple(file_paths, phrase, lambda_concurrency, local:
    """Print all lines in all files that the phrase occurs in.

    Args:
        file_paths: List of file paths to search.
        phrase: regex string to find matches of.
        lambda_concurrency: Max number of concurrent Lambda calls allowed.
    """

    pool = mp.Pool(processes = lambda_concurrency)

    if local:
        grep = grep_handler.grep_single
    else:
        grep = lambda_grep_single

    results = pool.starmap(
        grep,
        zip(
            file_paths,
            [phrase for file_path in file_paths],
            [local for file_path in file_paths]))
    pool.close()
    pool.join()
    return results


def generate_file_names():
    """Return the namespace of searchable files."""
    file_names = []
    for letter in string.ascii_lowercase:
        for digit in string.digits:
            file_name = "/docs/file-" + letter + digit + ".txt"
            file_names.append(file_name)
    return file_names


def main():
    parser = argparse.ArgumentParser(description = 'Demonstrate SFS with grep')
    parser.add_argument('--local', type = bool, required = False,
                        help = 'use SFS or local files')
    parser.add_argument('--num-files', type = int,
                        required = False, help = 'number of files to grep')
    args = parser.parse_args()

    if args.num_files:
        files = generate_file_names()[:args.num_files]
    else:
        files = ["/aeneid_test.txt",
                 "lambda_grep.py", "/aeneid_test2.txt"]

        # awe
    phrase = "awe"

    start = time.time()
    results = lambda_grep_multiple(files, phrase, 8, args.local)
    end = time.time()

    for result in results:
        print(result)
    print(end - start)


if __name__ == "__main__":
    main()
