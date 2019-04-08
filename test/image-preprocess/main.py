import argparse
import multiprocessing as mp
import os
import time

from PIL import Image, ImageEnhance

parser = argparse.ArgumentParser()
parser.add_argument("input_folder", help="folder of images to process")
parser.add_argument("output_folder", help="folder to save processed images in")

filters = [
    (ImageEnhance.Color, 0.0),
    (ImageEnhance.Contrast, 1.5),
    (ImageEnhance.Sharpness, 2.0),
]


def process_image(input_file, filters, output_file):
    """Apply filters to input_file and record changes in output_file."""
    image = Image.open(input_file)
    for enhancer, factor in filters:
        image = enhancer(image).enhance(factor)
    image.save(output_file, "JPEG")

    """
    duration = timeit.timeit(
        lambda: process_image(
            input_file,
            filters,
            output_file),
        number=1,
    )
    input_size = os.stat(input_file).st_size
    output_size = os.stat(output_file).st_size
    """


def process_image_arguments(input_folder, output_folder, filters, files):
    """Generator to process process_image arguments into a tuple."""
    for file_name in files:
        input_file = "{}/{}".format(input_folder, file_name)
        output_file = "{}/{}".format(output_folder, file_name)
        yield (input_file, filters, output_file)


def main():
    args = parser.parse_args()

    files = os.listdir(args.input_folder)
    files.sort()

    start_time = time.time()
    with mp.Pool(4) as pool:
        pool.starmap(process_image,
                     process_image_arguments(args.input_folder,
                                             args.output_folder,
                                             filters,
                                             files))
    end_time = time.time()

    print("number of images", len(files))
    print("duration: %.3f s" % (end_time - start_time))


if __name__ == "__main__":
    main()
