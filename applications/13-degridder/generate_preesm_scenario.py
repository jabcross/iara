#!/usr/bin/env python3
"""Generate Preesm .slam and .scenario files for the degridder application.

Produces a shared-memory star-topology .slam (every core connected to one
shared_mem node) and a matching .scenario that pins host/setup tasks to Core0
and allows degridder parallel tasks on all cores.

Usage:
    python3 generate_preesm_scenario.py \
        --num-cores NUM_CORES \
        --num-chunk NUM_CHUNK \
        --num-kernel-support NUM_KERNEL_SUPPORT \
        --size SIZE \
        --preesm-repo PREESM_REPO_PATH

Writes:
    {PREESM_REPO}/Archi/generated_{num_cores}cores.slam
    {PREESM_REPO}/Scenarios/generated_{size}_{num_cores}cores_{num_chunk}chunks_{ks}supports.scenario

Prints the scenario filename (basename only) to stdout on success.
"""

import argparse
from pathlib import Path

# ---------------------------------------------------------------------------
# Dataset-size constants — must match experiments.yaml computed_parameters
# ---------------------------------------------------------------------------
SIZE_PARAMS = {
    "small":  dict(grid_size=2560,  num_visibilities=3924480,  num_scenario=1),
    "medium": dict(grid_size=3840,  num_visibilities=5886720,  num_scenario=2),
    "large":  dict(grid_size=5120,  num_visibilities=7848960,  num_scenario=3),
}

# ---------------------------------------------------------------------------
# Tasks pinned to Core0 (non-parallelisable host actors)
# Derived from P12_large_32_8_64.scenario (the multi-core reference scenario).
# ---------------------------------------------------------------------------
CORE0_EXCLUSIVE_TASKS = [
    "top_degridder/config_struct_set_up",
    "top_degridder/visibility_host_set_up",
    "top_degridder/convert_vis_to_csv",
    "top_degridder/CUFFT_EXECUTE_FORWARD_C2C_actor",
    "top_degridder/fft_shift_complex_to_complex_actor",
    "top_degridder/fft_shift_real_to_complex_actor",
    "top_degridder/load_image_from_file",
    "top_degridder/generate_kernels",
]

# Tasks that every core (including Core0) may execute
PARALLEL_TASKS = [
    "top_degridder/degridder_parallel/Degridder",
    "top_degridder/degridder_parallel/iterator",
    "top_degridder/degridder_parallel",
    "top_degridder",
]

CORE0_TASKS = CORE0_EXCLUSIVE_TASKS + PARALLEL_TASKS
WORKER_TASKS = [
    "top_degridder/degridder_parallel/Degridder",
    "top_degridder/degridder_parallel/iterator",
    "top_degridder/degridder_parallel",
]


# ---------------------------------------------------------------------------
# .slam generator
# ---------------------------------------------------------------------------
def generate_slam(num_cores: int) -> str:
    """Shared-memory star topology: each CoreN ↔ shared_mem."""
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
# .scenario generator
# ---------------------------------------------------------------------------
def generate_scenario(
    num_cores: int,
    num_chunk: int,
    num_kernel_support: int,
    size: str,
    slam_name: str,
) -> str:
    params = SIZE_PARAMS[size]
    cores = [f"Core{i}" for i in range(num_cores)]

    # Core0 constraint group
    core0_xml = "\n".join(f"            <task name=\"{t}\"/>" for t in CORE0_TASKS)
    constraint_groups = f"""\
        <constraintGroup>
            <operator name="Core0"/>
{core0_xml}
        </constraintGroup>
"""

    # Worker cores (Core1..N-1)
    for core in cores[1:]:
        worker_xml = "\n".join(f"            <task name=\"{t}\"/>" for t in WORKER_TASKS)
        constraint_groups += f"""\
        <constraintGroup>
            <operator name="{core}"/>
{worker_xml}
        </constraintGroup>
"""

    return f"""\
<?xml version="1.0" encoding="UTF-8"?>
<scenario>
    <flags>
        <sizesAreInBit/>
    </flags>
    <files>
        <algorithm url="/degridder/Algo/top_parallel_degridder.pi"/>
        <architecture url="/degridder/Archi/{slam_name}"/>
        <codegenDirectory url="/degridder/Code/generated/"/>
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
            <dataType name="PRECISION2" size="128"/>
            <dataType name="PRECISION" size="64"/>
            <dataType name="int2" size="64"/>
            <dataType name="PRECISION3" size="192"/>
            <dataType name="Config" size="2432"/>
            <dataType name="double" size="64"/>
            <dataType name="int" size="32"/>
            <dataType name="float2" size="64"/>
            <dataType name="float3" size="96"/>
        </dataTypes>
        <specialVertexOperators>
            <specialVertexOperator path="Core0"/>
        </specialVertexOperators>
    </simuParams>
    <parameterValues>
        <parameter name="NUM_CHUNK" parent="degridder_parallel" value="{num_chunk}"/>
        <parameter name="NUM_KERNEL_SUPPORT" parent="top_degridder" value="{num_kernel_support}"/>
        <parameter name="GRID_SIZE" parent="top_degridder" value="{params['grid_size']}"/>
        <parameter name="NUM_VISIBILITIES" parent="top_degridder" value="{params['num_visibilities']}"/>
        <parameter name="NUM_SCENARIO" parent="top_degridder" value="{params['num_scenario']}"/>
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
    parser.add_argument("--num-chunk", type=int, required=True)
    parser.add_argument("--num-kernel-support", type=int, required=True)
    parser.add_argument("--size", choices=list(SIZE_PARAMS), required=True)
    parser.add_argument("--preesm-repo", required=True,
                        help="Path to the degridder Eclipse project root")
    args = parser.parse_args()

    repo = Path(args.preesm_repo)

    slam_name = f"generated_{args.num_cores}cores.slam"
    (repo / "Archi" / slam_name).write_text(generate_slam(args.num_cores))

    scenario_name = (
        f"generated_{args.size}_{args.num_cores}cores"
        f"_{args.num_chunk}chunks_{args.num_kernel_support}supports.scenario"
    )
    (repo / "Scenarios" / scenario_name).write_text(
        generate_scenario(
            args.num_cores, args.num_chunk, args.num_kernel_support,
            args.size, slam_name,
        )
    )

    print(scenario_name)


if __name__ == "__main__":
    main()
