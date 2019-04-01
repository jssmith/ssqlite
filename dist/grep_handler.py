import os
import re
import sys

import sfs


def grep(file_name, phrase, use_sfs=True):
    p = re.compile(phrase)
    matched_lines = []

    if use_sfs:
        file = sfs.open(file_name, mode='r')
    else:
        file = open(file_name, mode='r')

    try:
        for line in file:
            if p.search(line):
                matched_lines.append(file_name + ": " + line)
    finally:
        file.close()

    return matched_lines


def lambda_handler(event, context):
    """
    Adapt grep for usage by AWS Lambda.

    context: if non-nil uses SFS
    """

    use_sfs = context is not None
    if use_sfs:
        mount_point = os.environ["NFS4_SERVER"]
        sfs.mount(mount_point)

    return {
        "matches": grep(event["file_path"], event["phrase"], use_sfs),
        "file_name": event["file_path"],
    }
