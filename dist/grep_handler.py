import os
import re
import sys

from sfs import *


def grep(file_name, phrase):
    p = re.compile(phrase)
    matched_lines = []
    with open(file_name, mode='r') as f:
        for line in f:
            if p.search(line):
                matched_lines.append(file_name + ": " + line)
    return matched_lines


def lambda_handler(event, context):
    mount_point = os.environ["NFS4_SERVER"]
    mount(mount_point)
    print("event", event)
    return {
        "matches": grep(event["file_path"], event["phrase"]),
        "file_name": event["file_path"],
    }
