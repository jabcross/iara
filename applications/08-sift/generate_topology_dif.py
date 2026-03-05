#!/usr/bin/env python3
"""
Generate a SIFT topology.mlir with parallelism-level P substituted.

The base topology.dif has parallelismLevel=4 hardcoded in all rate fields.
This script produces a new DIF with every P-dependent rate scaled to the
requested parallelism level, then calls dif-to-iara.py to emit topology.mlir.

Usage:
    python3 generate_topology_dif.py <input.dif> <output.mlir> <P>

Where P is the parallelism level (number of parallel octave-range workers).
"""

import re
import sys
import os
import tempfile
import subprocess

# Actortypes whose ALL production int rates "= 4" should become P
ITERATOR_ACTORTYPES = {
    "ITERATOR_detect_keypoints",
    "ITERATOR_build_dog_pyr",
    "ITERATOR_build_grd_rot_pyr",
}


def compute_values(P):
    """Pre-compute all P-dependent numeric values for parallelism level P."""
    return {
        "P": P,
        # BdRot/BdGrd_BROADCAST forDetection/forExtraction = P x 8191500
        "forDetection_rot_grd": P * 8191500,
        "forExtraction_rot_grd": P * 8191500,
        # BdGpyr_BROADCAST forDogPyr/forGrdRotPyr = P x 16383000
        "forDogPyr": P * 16383000,
        "forGrdRotPyr": P * 16383000,
        # build_dog_pyr per-worker output = 13652500 // P
        "dogPyr_per_worker": 13652500 // P,
        # build_grd_rot_pyr per-worker outputs = 8191500 // P
        "grdPyr_per_worker": 8191500 // P,
        "rotPyr_per_worker": 8191500 // P,
        # MERGE_keypoints keypoints_in = P x 351
        "keypoints_in": P * 351,
        # BdCounterGpyrXOctave_BROADCAST forBlurN = P x nGpyrLayers(6)
        "forBlurN": P * 6,
        # BdDoG_BROADCAST forDetection = P x total_dogPyr(13652500)
        "forDetection_dog": P * 13652500,
        # BdBlurDown2x1_BROADCAST forRec = P x 128000
        "forRec_blur2x1": P * 128000,
    }


def transform_line(line, current_actortype, v):
    """Apply P-dependent substitutions to one DIF line."""
    P = v["P"]

    # ---- Global substitutions (field name + old value together are unique) ----

    # parallelismLevel parameter block value (always P)
    line = re.sub(r"(\bparallelismLevel\s*:\s*int\s*=\s*)4\b",
                  lambda m: m.group(1) + str(P), line)

    # BdRot/BdGrd_BROADCAST forDetection = 32766000 -> P x 8191500
    line = re.sub(r"(production\s+forDetection\s*:\s*float\s*=\s*)32766000\b",
                  lambda m: m.group(1) + str(v["forDetection_rot_grd"]), line)

    # BdRot/BdGrd_BROADCAST forExtraction = 32766000 -> P x 8191500
    line = re.sub(r"(production\s+forExtraction\s*:\s*float\s*=\s*)32766000\b",
                  lambda m: m.group(1) + str(v["forExtraction_rot_grd"]), line)

    # BdGpyr_BROADCAST forDogPyr = 65532000 -> P x 16383000
    line = re.sub(r"(production\s+forDogPyr\s*:\s*float\s*=\s*)65532000\b",
                  lambda m: m.group(1) + str(v["forDogPyr"]), line)

    # BdGpyr_BROADCAST forGrdRotPyr = 65532000 -> P x 16383000
    line = re.sub(r"(production\s+forGrdRotPyr\s*:\s*float\s*=\s*)65532000\b",
                  lambda m: m.group(1) + str(v["forGrdRotPyr"]), line)

    # build_dog_pyr per-worker dogPyr = 3413125 -> 13652500 // P
    line = re.sub(r"(production\s+dogPyr\s*:\s*float\s*=\s*)3413125\b",
                  lambda m: m.group(1) + str(v["dogPyr_per_worker"]), line)

    # build_grd_rot_pyr per-worker grdPyr = 2047875 -> 8191500 // P
    line = re.sub(r"(production\s+grdPyr\s*:\s*float\s*=\s*)2047875\b",
                  lambda m: m.group(1) + str(v["grdPyr_per_worker"]), line)

    # build_grd_rot_pyr per-worker rotPyr = 2047875 -> 8191500 // P
    line = re.sub(r"(production\s+rotPyr\s*:\s*float\s*=\s*)2047875\b",
                  lambda m: m.group(1) + str(v["rotPyr_per_worker"]), line)

    # MERGE_keypoints keypoints_in = 1404 -> P x 351
    line = re.sub(r"(consumption\s+keypoints_in\s*:\s*SiftKpt\s*=\s*)1404\b",
                  lambda m: m.group(1) + str(v["keypoints_in"]), line)

    # BdCounterGpyrXOctave_BROADCAST forBlurN = 24 -> P x 6
    line = re.sub(r"(production\s+forBlurN\s*:\s*int\s*=\s*)24\b",
                  lambda m: m.group(1) + str(v["forBlurN"]), line)

    # BdDoG_BROADCAST forDetection = 54610000 -> P x 13652500
    line = re.sub(r"(production\s+forDetection\s*:\s*float\s*=\s*)54610000\b",
                  lambda m: m.group(1) + str(v["forDetection_dog"]), line)

    # ---- Context-dependent substitutions (need actortype name) ----

    if current_actortype in ITERATOR_ACTORTYPES:
        # All production int rates = 4 -> P  (start/stop octave/layer/line/col)
        line = re.sub(r"(production\s+\w+\s*:\s*int\s*=\s*)4\b",
                      lambda m: m.group(1) + str(P), line)

    elif current_actortype == "BdCounterGpyr_BROADCAST":
        # for2x, forDirectGaussian, for2x_2, forDirectGaussian_2 = 4 -> P
        for field in ("for2x", "forDirectGaussian", "for2x_2", "forDirectGaussian_2"):
            line = re.sub(
                r"(production\s+" + re.escape(field) + r"\s*:\s*int\s*=\s*)4\b",
                lambda m: m.group(1) + str(P), line)

    elif current_actortype == "counterPLevels":
        # iter = 4 -> P  (counterOctaveDownN also has iter=4 but must stay nOctavesDownN=4)
        line = re.sub(r"(production\s+iter\s*:\s*int\s*=\s*)4\b",
                      lambda m: m.group(1) + str(P), line)

    elif current_actortype == "BdBlurDown2x1_BROADCAST":
        # forRec = 512000 -> P x 128000  (BdGT_BROADCAST also has forRec=512000 but fixed)
        line = re.sub(r"(production\s+forRec\s*:\s*float\s*=\s*)512000\b",
                      lambda m: m.group(1) + str(v["forRec_blur2x1"]), line)

    return line


def generate(input_dif, output_mlir, P):
    v = compute_values(P)

    with open(input_dif) as f:
        lines = f.readlines()

    result = []
    current_actortype = None
    brace_depth = 0

    for line in lines:
        # Detect actortype block start
        m = re.match(r"\s*actortype\s+(\w+)\s*\{", line)
        if m:
            current_actortype = m.group(1)
            brace_depth = 1
            result.append(transform_line(line, current_actortype, v))
            continue

        # Track brace depth while inside an actortype block
        if current_actortype is not None:
            brace_depth += line.count("{") - line.count("}")
            if brace_depth <= 0:
                result.append(transform_line(line, current_actortype, v))
                current_actortype = None
                brace_depth = 0
                continue

        result.append(transform_line(line, current_actortype, v))

    # Write modified DIF to temp file, then call dif-to-iara.py
    script_dir = os.path.dirname(os.path.abspath(__file__))
    iara_dir = os.path.abspath(os.path.join(script_dir, "..", ".."))
    dif_to_iara = os.path.join(iara_dir, "scripts", "dif-to-iara.py")

    fd, tmp_path = tempfile.mkstemp(suffix=".dif")
    try:
        with os.fdopen(fd, "w") as tmp:
            tmp.writelines(result)
        with open(output_mlir, "w") as out_f:
            subprocess.run(["python3", dif_to_iara, tmp_path],
                           stdout=out_f, check=True)
    finally:
        os.unlink(tmp_path)

    print(f"Generated {output_mlir} with parallelismLevel={P}", file=sys.stderr)


def main():
    if len(sys.argv) != 4:
        print(f"Usage: {sys.argv[0]} <input.dif> <output.mlir> <P>",
              file=sys.stderr)
        sys.exit(1)
    generate(sys.argv[1], sys.argv[2], int(sys.argv[3]))


if __name__ == "__main__":
    main()
