from PIL import ImageEnhance, Image
import PIL.Image
import PIL.ImageEnhance
import io
import sfs
import logging

DEFAULT_FILTERS = [
    (ImageEnhance.Color, 0.0),
]


def process_image(input_file, output_file, filters, local):
    """
    Apply series of filters to input_file and record changes in output_file.

    local (bool): A flag on whether to not use SFS
    """
    if local:
        image = Image.open(input_file)
    else:
        image = Image.open(sfs.open(input_file, "rb", buffering=2**20))

    logging.debug("starting filters")
    for enhancer, factor in filters:
        image = enhancer(image).enhance(factor)
    logging.debug("finishing filters")

    if local:
        image_out_file = open(output_file, "wb")
        image.save(image_out_file, "JPEG")
    else:
        image_out_file = sfs.open(output_file, "wb", buffering=2**20)
        image.save(image_out_file, "JPEG")


def process_images(input_files, output_files, filters, local):
    """Apply filters to multiple files."""
    logging.info("starting %s images", len(input_files))
    for i, input_file in enumerate(input_files):
        logging.info("processing image %s: %s", i, input_file)
        process_image(input_file, output_files[i], filters, local)
    logging.info("finished %s images", len(input_files))


def process_image_arguments(input_folder, output_folder, filters, local, files):
    """Generator to process process_image arguments into a tuple."""
    for file_name in files:
        input_file = "{}/{}".format(input_folder, file_name)
        output_file = "{}/{}".format(output_folder, file_name)
        yield (input_file, output_file, filters, local)

if __name__ == "__main__":
    import os

    logging.basicConfig(
            level=os.environ.get("LOGLEVEL", "DEBUG"),
            format="%(asctime)s\t%(levelname)s\t%(funcName)s\t%(message)s")

    mount_point = os.environ["NFS4_SERVER"]
    sfs.mount(mount_point)

    process_images(
            ["/before/cat.jpg", "/before/dog.jpg"],
            ["/after/cat.jpg", "/after/dog.jpg"], 
            DEFAULT_FILTERS, 
            False)

