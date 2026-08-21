#!/usr/bin/env python3
import sys
import argparse
import numpy as np
from astropy.io import fits


def fft_resample(img, n):
    """Bandlimited (FFT) resample to n x n, flux-preserving.

    Keeps the low-frequency half of the spectrum (center crop of the shifted
    transform); unitary normalization makes this the least-squares
    bandlimited interpolation — correct for non-integer ratios like
    16384 -> 5120.
    """
    h, w = img.shape
    F = np.fft.fftshift(np.fft.fft2(img, norm="ortho"))
    lo_r = (h - n) // 2
    lo_c = (w - n) // 2
    Fc = F[lo_r:lo_r + n, lo_c:lo_c + n]
    return np.fft.ifft2(np.fft.ifftshift(Fc), norm="ortho").real


def convert_fits_to_csv(fits_path, csv_path, grid_size):
    """
    Converts a FITS image to a single-column CSV for the degridder C application.

    Smaller images are center-padded with zeros to grid_size x grid_size;
    larger images are FFT-resampled down to grid_size x grid_size (the
    degridder's grid is fixed, so the sky image must be resampled, not
    cropped).  Matching the degridder's centered-coordinate convention
    (grid_center = GRID_SIZE / 2 in degridding.c).
    """
    with fits.open(fits_path) as hdul:
        data = hdul[0].data
        native_data = data.astype(data.dtype.newbyteorder('=')).squeeze()

    img_h, img_w = native_data.shape
    if img_h > grid_size or img_w > grid_size:
        print(f"Resampling {fits_path} ({img_h}x{img_w}) -> {grid_size}x{grid_size}")
        out = fft_resample(native_data, grid_size)
    else:
        # Center the image in a grid_size x grid_size zero array
        out = np.zeros((grid_size, grid_size), dtype=np.float32)
        row_off = (grid_size - img_h) // 2
        col_off = (grid_size - img_w) // 2
        out[row_off:row_off + img_h, col_off:col_off + img_w] = native_data

    np.savetxt(csv_path, out.flatten(), fmt='%f')
    print(f"Converted {fits_path} ({img_h}x{img_w}) -> {csv_path} "
          f"({grid_size}x{grid_size})")


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
