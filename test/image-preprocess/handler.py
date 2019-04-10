import os

import PIL
import process
import sfs

mount_point = os.environ["NFS4_SERVER"]
sfs.mount(mount_point)


def lambda_handler(event, context):
    input_files = event["input_files"]
    output_files = event["input_files"]
    process.process_image(
        input_files, process.DEFAULT_FILTERS, output_files, False)
