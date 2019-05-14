import argparse
import multiprocessing as mp
import os
import json
import time
import logging

import process

import boto3

lambda_client = boto3.client('lambda')

def lambda_process_images(input_files, output_files):
    """
    Call and return the result of an AWS Lambda call of grep on file_path.
    Returns:
        a dictionary of the Lambda response
        on success this will  include "matches",
        on error this will include "error_msg"
    """
    input_event = {
        "input_files": input_files,
        "output_files": output_files,
    }

    logging.info("starting lambda")
    invoke_response = lambda_client.invoke(
        FunctionName="sfs-image-preprocess",
        InvocationType='RequestResponse',
        Payload=json.dumps(input_event)
    )
    logging.info("finished lambda")

    payload = invoke_response['Payload'].read()
    return json.loads(payload.decode("utf-8"))

def distributed_processing_args(input_folder, output_folder, files, num_lambda):
    """Create NUM_LAMBDA sets of input files and output files."""
    divided_files = divide_files(files, num_lambda)

    lambda_pool_args = []
    for file_name_chunk in divided_files:
        lambda_thread_input = []
        lambda_thread_output = []
        for file_name in file_name_chunk:
            lambda_thread_input.append("{}/{}".format(input_folder, file_name))
            lambda_thread_output.append("{}/{}".format(output_folder, file_name))
        lambda_pool_args.append((lambda_thread_input, lambda_thread_output))
    return lambda_pool_args

def distributed_processing(input_folder, output_folder, files, num_lambda):
    """Process the images using AWS Lambda concurrently."""
    lambda_pool_args = distributed_processing_args(
            input_folder,
            output_folder,
            files,
            num_lambda)

    with mp.Pool(num_lambda) as pool:
        return pool.starmap(lambda_process_images, lambda_pool_args)

    return -1


def divide_files(files, divisor):
    """
    Partions files into DIVISOR partitions.

    Returns a list of lists of the original FILES
    """
    assigned_files = [[] for i in range(divisor)]
    for i, file_name in enumerate(files):
        key = i % divisor
        assigned_files[key].append(file_name)
    return assigned_files




def distributed_main():
    args = parser.parse_args()
    files = ["{:05d}.jpg".format(num) for num in range(0, 1000)]

    start_time = time.time()
    results = distributed_processing(
            args.input_folder,
            args.output_folder,
            files,
            args.num_threads)
    end_time = time.time()

    for x in results:
        print(x)

    print("number of images", len(files))
    print("duration: %.3f s" % (end_time - start_time))

def single_processing_args(input_folder, output_folder, filters, local, files):
    """Generator to process process_image arguments into a tuple."""
    for file_name in files:
        input_file = "{}/{}".format(input_folder, file_name)
        output_file = "{}/{}".format(output_folder, file_name)
        yield (input_file, output_file, filters, local)

def single_machine_processing(args, files):
    """Process the images on a single machine."""
    with mp.Pool(args.num_threads) as pool:
        pool.starmap(
            process.process_image,
            single_processings_args(
                args.input_folder,
                args.output_folder,
                process.DEFAULT_FILTERS,
                True,
                files))

def single_main():
    args = parser.parse_args()

    files = os.listdir(args.input_folder)
    files.sort()

    start_time = time.time()
    single_machine_processing(args, files)
    end_time = time.time()

    print("number of images", len(files))
    print("duration: %.3f s" % (end_time - start_time))

if __name__ == "__main__":

    parser = argparse.ArgumentParser()
    parser.add_argument("input_folder", help="folder of images to process")
    parser.add_argument("output_folder", help="folder to save processed images in")
    parser.add_argument("num_threads", type=int, help="num of threads to use")

    distributed_main()
