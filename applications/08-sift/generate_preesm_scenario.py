#!/usr/bin/env python3
"""Generate Preesm .slam and .scenario files for the SIFT application.

Produces a shared-memory star-topology .slam (every core connected to one
shared_mem node) and a matching .scenario that allows all SIFT tasks on
every core (no Core0-exclusive pinning — Preesm's scheduler assigns freely).

All parameters are hardcoded in Hextract.pi; the scenario uses an empty
<parameterValues/> element.

Usage:
    python3 generate_preesm_scenario.py \
        --num-cores NUM_CORES \
        --preesm-repo PREESM_SIFT_REPO_PATH

Writes:
    {repo}/Archi/generated_{N}cores.slam
    {repo}/Scenarios/generated_{N}cores.scenario
Prints the scenario filename (basename only) to stdout on success.
"""

import argparse
from pathlib import Path

# ---------------------------------------------------------------------------
# All 84 SIFT task names (from HextractSIFT4Corex86_64.scenario, Core0 group)
# Every core gets access to all tasks so Preesm can schedule freely.
# ---------------------------------------------------------------------------
ALL_TASKS = [
    "Hextract/BdFilename",
    "Hextract/read_pgm",
    "Hextract/BdOriginalImage",
    "Hextract/draw_keypoints_to_ppm_file",
    "Hextract/filename1",
    "Hextract/BdKeypoints",
    "Hextract/BdNbKeypoints",
    "Hextract/export_keypoints_to_key_file",
    "Hextract/SIFT/compute_gaussian_coefs",
    "Hextract/SIFT/extract_descriptor",
    "Hextract/SIFT/detect_keypoints",
    "Hextract/SIFT/ITERATOR_detect_keypoints",
    "Hextract/SIFT/BdRot",
    "Hextract/SIFT/BdGrd",
    "Hextract/SIFT/build_dog_pyr",
    "Hextract/SIFT/ITERATOR_build_dog_pyr",
    "Hextract/SIFT/build_grd_rot_pyr",
    "Hextract/SIFT/ITERATOR_build_grd_rot_pyr",
    "Hextract/SIFT/to_float",
    "Hextract/SIFT/BdGpyr",
    "Hextract/SIFT/MERGE_gpyr",
    "Hextract/SIFT/BdFloatImg",
    "Hextract/SIFT/upsample2x",
    "Hextract/SIFT/downsample2xN",
    "Hextract/SIFT/BdCoefs",
    "Hextract/SIFT/BdSizes",
    "Hextract/SIFT/BdBlurUp2x",
    "Hextract/SIFT/BdBlurDown2x1",
    "Hextract/SIFT/seq_blur1",
    "Hextract/SIFT/downsample2x1",
    "Hextract/SIFT/seq_blurN",
    "Hextract/SIFT/BdBlurDown2xN",
    "Hextract/SIFT/counterGpyrLayer",
    "Hextract/SIFT/BdCounterGpyr",
    "Hextract/SIFT/counterOctaveDownN",
    "Hextract/SIFT/keypoints",
    "Hextract/SIFT/image",
    "Hextract/SIFT/BdGT",
    "Hextract/SIFT/BdGT_2x",
    "Hextract/SIFT/BdBlurred1",
    "Hextract/SIFT/BdBlurredN",
    "Hextract/SIFT/BdOctaveDown",
    "Hextract/SIFT/nbKeypoints",
    "Hextract/SIFT/MERGE_keypoints",
    "Hextract/SIFT/BdCounterGpyrXOctave",
    "Hextract/SIFT/BdDoG",
    "Hextract/SIFT/BarrierCounterGpyr",
    "Hextract/SIFT/SPLIT_upsample2x",
    "Hextract/SIFT/counterPLevels",
    "Hextract/SIFT/delay_BdBlurDown2xN_forRec__downsample2xN_imgDownPrev",
    "Hextract/SIFT/delay_BdBlurred1_forRec__seq_blur1_imgBlurPrev",
    "Hextract/SIFT/delay_BdBlurredN_forRec__seq_blurN_imgBlurPrev",
    "Hextract/SIFT/delay_BdGT_forRec__Blur_iterPrev",
    "Hextract/SIFT/delay_BdGT_2x_forRec__Blur2x_iterPrev",
    "Hextract/SIFT/Blur2x/row_filter_transpose2x_1",
    "Hextract/SIFT/Blur2x/row_filter_transpose2x_2",
    "Hextract/SIFT/Blur2x/BarrierTranspose2x_1",
    "Hextract/SIFT/Blur2x/BarrierTranspose2x_2",
    "Hextract/SIFT/Blur2x/imgBlurred",
    "Hextract/SIFT/Blur2x/gauss_coefs1",
    "Hextract/SIFT/Blur2x/imgOri",
    "Hextract/SIFT/Blur2x/col_sizes1",
    "Hextract/SIFT/Blur2x/iterPrev",
    "Hextract/SIFT/Blur2x/iter_nb1",
    "Hextract/SIFT/Blur2x/col_sizes2",
    "Hextract/SIFT/Blur2x/gauss_coefs2",
    "Hextract/SIFT/Blur2x/iter_nb2",
    "Hextract/SIFT/Blur2x",
    "Hextract/SIFT/Blur/row_filter_transpose_1",
    "Hextract/SIFT/Blur/row_filter_transpose_2",
    "Hextract/SIFT/Blur/BarrierTranspose_1",
    "Hextract/SIFT/Blur/BarrierTranspose_2",
    "Hextract/SIFT/Blur/imgBlurred",
    "Hextract/SIFT/Blur/gauss_coefs1",
    "Hextract/SIFT/Blur/imgOri",
    "Hextract/SIFT/Blur/col_sizes1",
    "Hextract/SIFT/Blur/iterPrev",
    "Hextract/SIFT/Blur/iter_nb1",
    "Hextract/SIFT/Blur/iter_nb2",
    "Hextract/SIFT/Blur/gauss_coefs2",
    "Hextract/SIFT/Blur/col_sizes2",
    "Hextract/SIFT/Blur",
    "Hextract/SIFT",
    "Hextract",
]


# ---------------------------------------------------------------------------
# .slam generator (shared-memory star topology — identical to degridder)
# ---------------------------------------------------------------------------
def generate_slam(num_cores: int) -> str:
    """Shared-memory star topology: each CoreN <-> shared_mem."""
    cores = [f"Core{i}" for i in range(num_cores)]

    core_instances = ""
    for i, core in enumerate(cores):
        core_instances += f"""\
        <spirit:componentInstance>
            <spirit:instanceName>{core}</spirit:instanceName>
            <spirit:hardwareId>{i}</spirit:hardwareId>
            <spirit:componentRef spirit:library="" spirit:name="x86" spirit:vendor="" spirit:version=""/>
            <spirit:configurableElementValues/>
        </spirit:componentInstance>
"""

    interconnections = ""
    link_descriptions = ""
    for core in cores:
        link_name = f"{core}|shared_mem|shared_mem|shared_mem"
        interconnections += f"""\
        <spirit:interconnection>
            <spirit:name>{link_name}</spirit:name>
            <spirit:activeInterface spirit:busRef="shared_mem" spirit:componentRef="{core}"/>
            <spirit:activeInterface spirit:busRef="shared_mem" spirit:componentRef="shared_mem"/>
        </spirit:interconnection>
"""
        link_descriptions += f"""\
            <slam:linkDescription slam:directedLink="undirected" slam:linkType="DataLink" slam:referenceId="{link_name}"/>
"""

    return f"""\
<?xml version="1.0" encoding="UTF-8"?>
<spirit:design xmlns:spirit="http://www.spiritconsortium.org/XMLSchema/SPIRIT/1.4">
    <spirit:vendor>ietr</spirit:vendor>
    <spirit:library>preesm</spirit:library>
    <spirit:name>generated_{num_cores}CoresX86</spirit:name>
    <spirit:version>1</spirit:version>
    <spirit:componentInstances>
{core_instances}        <spirit:componentInstance>
            <spirit:instanceName>shared_mem</spirit:instanceName>
            <spirit:hardwareId>0</spirit:hardwareId>
            <spirit:componentRef spirit:library="" spirit:name="SHARED_MEM" spirit:vendor="" spirit:version=""/>
            <spirit:configurableElementValues/>
        </spirit:componentInstance>
    </spirit:componentInstances>
    <spirit:interconnections>
{interconnections}    </spirit:interconnections>
    <spirit:hierConnections/>
    <spirit:vendorExtensions>
        <slam:componentDescriptions xmlns:slam="http://sourceforge.net/projects/dftools/slam">
            <slam:componentDescription slam:componentRef="x86" slam:componentType="CPU" slam:refinement=""/>
            <slam:componentDescription slam:componentRef="SHARED_MEM" slam:componentType="parallelComNode" slam:refinement="" slam:speed="1000000000"/>
        </slam:componentDescriptions>
        <slam:linkDescriptions xmlns:slam="http://sourceforge.net/projects/dftools/slam">
{link_descriptions}        </slam:linkDescriptions>
        <slam:designDescription xmlns:slam="http://sourceforge.net/projects/dftools/slam">
            <slam:parameters/>
        </slam:designDescription>
    </spirit:vendorExtensions>
</spirit:design>
"""


# ---------------------------------------------------------------------------
# .scenario generator (all tasks on all cores; empty parameterValues)
# ---------------------------------------------------------------------------
def generate_scenario(num_cores: int, slam_name: str) -> str:
    cores = [f"Core{i}" for i in range(num_cores)]

    constraint_groups = ""
    for core in cores:
        task_xml = "\n".join(f"            <task name=\"{t}\"/>" for t in ALL_TASKS)
        constraint_groups += f"""\
        <constraintGroup>
            <operator name="{core}"/>
{task_xml}
        </constraintGroup>
"""

    return f"""\
<?xml version="1.0" encoding="UTF-8"?>
<scenario>
    <flags>
        <sizesAreInBit/>
    </flags>
    <files>
        <algorithm url="/SIFTapp/Algo/Hextract.pi"/>
        <architecture url="/SIFTapp/Archi/{slam_name}"/>
        <codegenDirectory url="/SIFTapp/Code/generated/"/>
    </files>
    <constraints excelUrl="">
{constraint_groups}    </constraints>
    <timings excelUrl="">
        <memcpyspeed opname="x86" setuptime="1" timeperunit="1.0E-10"/>
    </timings>
    <simuParams>
        <mainCore>Core0</mainCore>
        <mainComNode>shared_mem</mainComNode>
        <averageDataSize>1000</averageDataSize>
        <dataTypes>
            <dataType name="SiftKpt" size="4448"/>
            <dataType name="char" size="8"/>
            <dataType name="float" size="32"/>
            <dataType name="unsigned char" size="8"/>
            <dataType name="int" size="32"/>
        </dataTypes>
        <specialVertexOperators>
            <specialVertexOperator path="Core0"/>
        </specialVertexOperators>
    </simuParams>
    <parameterValues>
        <parameter parent="Hextract" name="parallelismLevel" value="{num_cores}" type="PARAMETER"/>
    </parameterValues>
    <papifyConfigs xmlUrl=""/>
    <energyConfigs xmlUrl="">
        <performanceObjective objectiveEPS="0.0"/>
        <pePower opName="Base" pePower="10.0"/>
        <pePower opName="x86" pePower="10.0"/>
        <peActorsEnergy/>
    </energyConfigs>
</scenario>
"""


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--num-cores", type=int, required=True)
    parser.add_argument("--preesm-repo", required=True,
                        help="Path to the SIFT Eclipse project root (preesm-apps/SIFT)")
    args = parser.parse_args()

    repo = Path(args.preesm_repo)

    slam_name = f"generated_{args.num_cores}cores.slam"
    (repo / "Archi" / slam_name).write_text(generate_slam(args.num_cores))

    scenario_name = f"generated_{args.num_cores}cores.scenario"
    (repo / "Scenarios" / scenario_name).write_text(
        generate_scenario(args.num_cores, slam_name)
    )

    print(scenario_name)


if __name__ == "__main__":
    main()
