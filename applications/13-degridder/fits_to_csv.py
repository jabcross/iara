#!/usr/bin/env python3
import sys
import argparse
import numpy as np
from astropy.io import fits


def convert_fits_to_csv(fits_path, csv_path, grid_size):
    """
    Converts a FITS image to a single-column CSV for the degridder C application.

    The FITS image (e.g. 2048x2048) is center-padded with zeros to the target
    grid_size x grid_size, matching the degridder's centered-coordinate convention
    (grid_center = GRID_SIZE / 2 in degridding.c).
    """
    with fits.open(fits_path) as hdul:
        data = hdul[0].data
        native_data = data.astype(data.dtype.newbyteorder('=')).squeeze()

    img_h, img_w = native_data.shape
    if img_h > grid_size or img_w > grid_size:
        raise ValueError(
            f"FITS image ({img_h}x{img_w}) is larger than grid_size ({grid_size}). "
            "Cannot center-pad."
        )

    # Center the image in a grid_size x grid_size zero array
    padded = np.zeros((grid_size, grid_size), dtype=np.float32)
    row_off = (grid_size - img_h) // 2
    col_off = (grid_size - img_w) // 2
    padded[row_off:row_off + img_h, col_off:col_off + img_w] = native_data

    np.savetxt(csv_path, padded.flatten(), fmt='%f')
    print(f"Converted {fits_path} ({img_h}x{img_w}) -> {csv_path} "
          f"({grid_size}x{grid_size}, center-padded, offset=({row_off},{col_off}))")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Convert a FITS image to a flat CSV, center-padded to a target grid size."
    )
    parser.add_argument("fits_file", help="Input FITS file")
    parser.add_argument("csv_file", help="Output CSV file")
    parser.add_argument("--grid-size", type=int, default=2560,
                        help="Target grid size (default: 2560)")
    args = parser.parse_args()
    convert_fits_to_csv(args.fits_file, args.csv_file, args.grid_size)
