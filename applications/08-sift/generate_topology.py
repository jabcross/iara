#!/usr/bin/env python3
"""
Generate topology.mlir for 08-sift with a given SIFT_PARALLELISM_LEVEL.

The P=4 topology.mlir in the source tree is the reference. This script
reads it and substitutes all P-dependent tensor sizes with values computed
for the requested parallelism level.

Fixed image parameters (must match topology.mlir source):
  IMAGE_WIDTH=800, IMAGE_HEIGHT=640, IMG_DOUBLE=1
  NUM_OCTAVES=7, N_GPYR=6, NLAYERS=3, NUM_DOG_LAYERS=5, KPT_MAX=351

Valid parallelism levels P must divide GCD(GRD_ROT_TOTAL, DOG_TOTAL) = 2730500.
The paper values {1, 2, 4, 5, 10} all satisfy this.
"""

import argparse
import sys
import os

# ---------------------------------------------------------------------------
# Fixed SIFT parameters
# ---------------------------------------------------------------------------
IMAGE_WIDTH   = 800
IMAGE_HEIGHT  = 640
IMG_DOUBLE    = 1
NUM_OCTAVES   = 7
N_GPYR        = 6
NLAYERS       = 3
NUM_DOG       = 5
KPT_MAX       = 351   # max keypoints per detect_keypoints call (P=1 value)
GAUSS_COEFS   = 126   # size of Gaussian coefficient array
COL_SIZES     = 6     # size of col_sizes array
OCTAVES_DOWN_N = 4    # FIXED algorithm constant, NOT the parallelism level

# Compute total pixel count across all octaves
def _total_pixels(width, height, img_double, num_octaves):
    w, h = width, height
    if img_double:
        w *= 2
        h *= 2
    total = 0
    for _ in range(num_octaves):
        total += w * h
        w //= 2
        h //= 2
    return total

TOTAL_PIX    = _total_pixels(IMAGE_WIDTH, IMAGE_HEIGHT, IMG_DOUBLE, NUM_OCTAVES)
GPYR_TOTAL   = N_GPYR   * TOTAL_PIX   # 16383000
GRD_ROT_TOTAL = NLAYERS * TOTAL_PIX   # 8191500
DOG_TOTAL    = NUM_DOG  * TOTAL_PIX   # 13652500

# Reference parallelism level in the source topology.mlir
P_REF = 4

def generate(parallelism: int, source_mlir: str) -> str:
    """Return topology.mlir text for the given parallelism level."""
    if GPYR_TOTAL % parallelism != 0 or GRD_ROT_TOTAL % parallelism != 0 \
            or DOG_TOTAL % parallelism != 0:
        raise ValueError(
            f"P={parallelism} does not divide all pyramid sizes "
            f"(GPYR={GPYR_TOTAL}, GRD_ROT={GRD_ROT_TOTAL}, DOG={DOG_TOTAL}). "
            f"Valid divisors of GCD={TOTAL_PIX} include {{1,2,4,5,7,10,14,...}}."
        )

    with open(source_mlir) as f:
        text = f.read()

    # ------------------------------------------------------------------
    # Step 1: protect tensor sizes that use "4" for OCTAVES_DOWN_N
    # (not the parallelism level) so they survive the global P substitution.
    # ------------------------------------------------------------------
    GUARD = "__OCTAVES_DOWN_N__"
    text = text.replace(
        f"@counterOctaveDownN out ( tensor<{P_REF}xi32> )",
        f"@counterOctaveDownN out ( tensor<{GUARD}xi32> )"
    )
    text = text.replace(
        f"%e55_d = iara.edge %e55_s : tensor<{P_REF}xi32>",
        f"%e55_d = iara.edge %e55_s : tensor<{GUARD}xi32>"
    )

    # ------------------------------------------------------------------
    # Step 2: substitute all P-dependent sizes (P_REF=4 → parallelism)
    # ------------------------------------------------------------------
    P = parallelism

    def s(old, new):
        nonlocal text
        text = text.replace(old, new)

    # Integer token streams produced by iterators / counters (xi32)
    s(f"tensor<{P_REF}xi32>",    f"tensor<{P}xi32>")

    # Per-worker pyramid slices (one slice out of P)
    s(f"tensor<{DOG_TOTAL // P_REF}xf32>",    f"tensor<{DOG_TOTAL // P}xf32>")
    s(f"tensor<{GRD_ROT_TOTAL // P_REF}xf32>", f"tensor<{GRD_ROT_TOTAL // P}xf32>")

    # Broadcast outputs that replicate data for P detect_keypoints workers
    s(f"tensor<{P_REF * GRD_ROT_TOTAL}xf32>", f"tensor<{P * GRD_ROT_TOTAL}xf32>")
    s(f"tensor<{P_REF * DOG_TOTAL}xf32>",     f"tensor<{P * DOG_TOTAL}xf32>")

    # Broadcast output that distributes the full Gaussian pyramid to P workers
    s(f"tensor<{P_REF * GPYR_TOTAL}xf32>",    f"tensor<{P * GPYR_TOTAL}xf32>")

    # Gaussian coefs / col_sizes broadcast for P parallel blur workers
    # Each worker needs N_GPYR copies → broadcast factor = P * N_GPYR
    s(f"tensor<{P_REF * N_GPYR * GAUSS_COEFS}xf32>",
      f"tensor<{P * N_GPYR * GAUSS_COEFS}xf32>")
    s(f"tensor<{P_REF * N_GPYR * COL_SIZES}xi32>",
      f"tensor<{P * N_GPYR * COL_SIZES}xi32>")

    # MERGE_keypoints accumulates P × KPT_MAX keypoint structs
    s(f"tensor<{P_REF * KPT_MAX}x!",          f"tensor<{P * KPT_MAX}x!")

    # ------------------------------------------------------------------
    # Step 3: restore protected OCTAVES_DOWN_N sizes
    # ------------------------------------------------------------------
    text = text.replace(f"tensor<{GUARD}xi32>", f"tensor<{P_REF}xi32>")

    return text


def main():
    parser = argparse.ArgumentParser(
        description="Generate SIFT topology.mlir for a given parallelism level"
    )
    parser.add_argument(
        "--parallelism-level", type=int, required=True,
        metavar="P",
        help="SIFT_PARALLELISM_LEVEL (number of parallel workers)"
    )
    parser.add_argument(
        "--output", required=True,
        metavar="PATH",
        help="Path to write the generated topology.mlir"
    )
    parser.add_argument(
        "--source",
        default=os.path.join(os.path.dirname(__file__), "topology.mlir"),
        metavar="PATH",
        help="Source P=4 topology.mlir template (default: topology.mlir next to this script)"
    )
    args = parser.parse_args()

    try:
        result = generate(args.parallelism_level, args.source)
    except ValueError as e:
        print(f"ERROR: {e}", file=sys.stderr)
        sys.exit(1)

    os.makedirs(os.path.dirname(os.path.abspath(args.output)), exist_ok=True)
    with open(args.output, "w") as f:
        f.write(result)

    print(f"Generated topology.mlir: P={args.parallelism_level}, output={args.output}")


if __name__ == "__main__":
    main()
