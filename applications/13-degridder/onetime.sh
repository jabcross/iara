#!/bin/sh
set -e

# Get the directory of the script
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
DATA_DIR="$SCRIPT_DIR/data"
LOCK_FILE="$SCRIPT_DIR/.download_complete"

# Exit if the data has already been downloaded
if [ -f "$LOCK_FILE" ]; then
    echo "Degridder data has already been downloaded. Skipping."
    exit 0
fi

echo "Degridder data not found. Downloading..."

# Check for gdown
if ! command -v gdown >/dev/null 2>&1; then
    echo "Error: gdown is not installed. Please run 'pip install gdown'." >&2
    exit 1
fi

# Check for unzip
if ! command -v unzip >/dev/null 2>&1; then
    echo "Error: unzip is not installed. Please install it to continue." >&2
    exit 1
fi

# Create data directory
mkdir -p "$DATA_DIR"

# Download the folder from Google Drive
# Folder ID for https://drive.google.com/drive/folders/1tq8jF0myYyk2BRgAaAyFlb4PcGzWXUtB
FOLDER_ID="1tq8jF0myYyk2BRgAaAyFlb4PcGzWXUtB"

echo "Downloading data from Google Drive... (this may take a while)"
gdown --folder "$FOLDER_ID" -O "$DATA_DIR"

# Ensure the correct structure: create 'input' directory and move relevant files
INPUT_DIR="$DATA_DIR/input"
mkdir -p "$INPUT_DIR"

echo "Structuring downloaded data..."

# Move cycle_0_deconvolved_*.csv files to input/
mv "$DATA_DIR"/cycle_0_deconvolved_*.csv "$INPUT_DIR" 2>/dev/null || true

# Unzip sim_*.ms.zip files and move them to data/
# The unzipped files will create directories like sim_small.ms, sim_medium.ms, sim_large.ms
for zip_file in "$DATA_DIR"/sim_*.ms.zip; do
    if [ -f "$zip_file" ]; then
        unzip -q "$zip_file" -d "$DATA_DIR"
        rm "$zip_file"
    fi
done

# Create input/kernels directory and move kernel files
KERNEL_DIR="$INPUT_DIR/kernels/small" # The original paths in top.c indicated a 'small' subdirectory under kernels
mkdir -p "$KERNEL_DIR"
mv "$DATA_DIR"/w-proj_supports_x16_2458_image.csv "$KERNEL_DIR" 2>/dev/null || true
mv "$DATA_DIR"/w-proj_kernels_imag_x16_2458_image.csv "$KERNEL_DIR" 2>/dev/null || true
mv "$DATA_DIR"/w-proj_kernels_real_x16_2458_image.csv "$KERNEL_DIR" 2>/dev/null || true

# Run the conversion script
echo "Converting FITS image to CSV..."
python3 "$SCRIPT_DIR/fits_to_csv.py" "$DATA_DIR/img_wproj_small_I.fits" "$INPUT_DIR/cycle_0_deconvolved_2560.csv" --grid-size 2560

# Comment out the cleanup step to avoid re-downloading during debugging
# rm -f "$DATA_DIR"/*.zip "$DATA_DIR"/*.npz "$DATA_DIR"/*.fits "$DATA_DIR"/*.txt "$DATA_DIR"/README.md

# Create the lock file to prevent re-running
touch "$LOCK_FILE"

echo "Degridder data structuring complete."
