"""
Simple grep implementation for native and AWS Lambda usage.

Provides the utility of grep on single files.
"""
import os
import re
import sys

import sfs


def grep_single(file_name, phrase, local):
    p = re.compile(phrase)
    matched_lines = []

    if local:
        try:
            file = open(file_name, mode='r')
        except FileNotFoundError as err:
            return err
    else:
        file = sfs.open(file_name, mode='r')

    try:
        for line in file:
            if p.search(line):
                matched_lines.append(line)
    finally:
        file.close()

    return matched_lines


def lambda_handler(event, context):
    """
    Adapt grep for usage by AWS Lambda.

    context: if non-nil uses SFS
    """

    mount_point = os.environ["NFS4_SERVER"]
    sfs.mount(mount_point)

    return {
        "matches": grep_single(event["file_path"], event["phrase"], False),
        "file_name": event["file_path"],
    }
