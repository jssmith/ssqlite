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
        f = sfs.open(input_file, "rb", buffering=2**20)
        image = Image.open(f)

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
    if type(input_files) is str or type(output_files) is str:
        raise ValueError("input_files and output_files must be a collection of strings")

    finished_images = []
    logging.info("starting %s images", len(input_files))
    for i, input_file in enumerate(input_files):
        logging.info("processing image %s: %s", i, input_file)
        try:
            process_image(input_file, output_files[i], filters, local)
            finished_images.append(output_files[i])
        except Exception as err:
            logging.error("failed to process image %s: %s", input_file, err)
    logging.info("finished %s images", len(input_files))
    return finished_images


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

