import os

import process
import sfs
import logging

def setup_logging():
    """Setup logging that outputs to Cloudwatch."""
    root = logging.getLogger()
    if root.handlers:
        for handler in root.handlers:
            root.removeHandler(handler)

    logging.basicConfig(
            level=os.environ.get("LOGLEVEL", "DEBUG"),
            format="%(asctime)s\t%(levelname)s\t%(funcName)s\t%(message)s")
    logging.info("starting")

def lambda_handler(event, context):
    input_files = event["input_files"]
    output_files = event["output_files"]
    logging.info("input_files: %s", input_files)
    logging.info("output_files: %s", output_files)

    process.process_images(
        input_files, output_files, process.DEFAULT_FILTERS, False)
    logging.info("finished")

setup_logging()

mount_point = os.environ["NFS4_SERVER"]
sfs.mount(mount_point)

if __name__ == "__main__":
    lambda_handler({
        "input_files": ["/cat.jpg"],
        "output_files": ["/a_cat.jpg"]},
        None)
