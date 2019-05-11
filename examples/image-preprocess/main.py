import argparse
import multiprocessing as mp
import os
import json
import time
import logging

import handler
import process

import boto3

lambda_client = boto3.client('lambda')



parser = argparse.ArgumentParser()
parser.add_argument("input_folder", help="folder of images to process")
parser.add_argument("output_folder", help="folder to save processed images in")


def single_machine_processing(args, files):
    """Process the images on a single machine."""
    with mp.Pool(4) as pool:
        pool.starmap(
            process.process_image,
            process.process_image_arguments(
                args.input_folder,
                args.output_folder,
                process.DEFAULT_FILTERS,
                True,
                files))


def distributed_processing(args, files):
    """Process the images using AWS Lambda concurrently."""
    num_lambda = 80
    assigned_files = divide_files(files, num_lambda)

    # Call lambda function


def divide_files(files, divisor):
    assigned_files = {}
    for i, file_name in enumerate(files):
        key = i % divisor
        if key not in assigned_files:
            assigned_files[key] = [file_name]
        else:
            assigned_files[key].append(file_name)
    return assigned_files


def lambda_process_images(input_files, output_files):
    """Call and return the result of an AWS Lambda call of grep on file_path.
    Returns:
        a dictionary of the Lambda response
        on success this will  include "matches",
        on error this will include "error_msg"
    """
    input_event = {
        "input_files": input_files,
        "output_files": output_files,
    }

    print("starting lambda")
    invoke_response = lambda_client.invoke(
        FunctionName="sfs-image-preprocess",
        InvocationType='RequestResponse',
        Payload=json.dumps(input_event)
    )
    print("finished lambda")
    return json.loads(invoke_response['Payload'].read().decode("utf-8"))

def main():
    args = parser.parse_args()

    files = os.listdir(args.input_folder)
    files.sort()

    start_time = time.time()
    if False:
        single_machine_processing(args, files)
    else:
        distributed_processing(args, files)
    end_time = time.time()

    print("number of images", len(files))
    print("duration: %.3f s" % (end_time - start_time))


if __name__ == "__main__":
    lambda_process_images(["/cat.jpg"], ["/lambda_cat.jpg"])
    #main()
