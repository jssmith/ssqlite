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
    """Apply series of filters to input_file and record changes in output_file."""
    if local:
        image = Image.open(input_file)
    else:
        image_file = sfs.open(input_file, "rb")
        image = Image.open(io.BytesIO(image_file.read()))

    logging.info("starting filters")
    for enhancer, factor in filters:
        image = enhancer(image).enhance(factor)
    logging.info("finishing filters")

    if local:
        image_out_file = open(output_file, "wb")
        image.save()
    else:
        temp_out_file = io.BytesIO()
        image.save(temp_out_file, "JPEG")
        temp_out_file.seek(0)

        image_out_file = sfs.open(output_file, "wb")
        image_out_file.write(temp_out_file.read())


def process_images(input_files, output_files, filters, local):
    """Apply filters to multiple files."""
    for i, input_file in enumerate(input_files):
        logging.info("processing image %s: %s", i, input_file)
        process_image(input_file, output_files[i], filters, local)


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

