# -*- Python -*-

import os
import platform
import re
import subprocess
import tempfile

import lit.formats
import lit.util

from lit.llvm import llvm_config
from lit.llvm.subst import ToolSubst
from lit.llvm.subst import FindTool

# Configuration file for the 'lit' test runner.

# name: The name of this test suite.
config.name = "IARA"

config.test_format = lit.formats.ShTest("0")

# suffixes: A list of file extensions to treat as test files.
config.suffixes = [".test"]

# test_source_root: The root path where tests are located.
config.test_source_root = os.path.dirname(__file__)

# test_exec_root: The root path where tests should be run.
config.test_exec_root = os.path.join(config.iara_obj_root, "test")

config.substitutions.append(("%PATH%", config.environment["PATH"]))
config.substitutions.append(("%shlibext", config.llvm_shlib_ext))

llvm_config.with_system_environment(
    ["HOME", "INCLUDE", "LIB", "TMP", "TEMP", "IARA_DIR", "LLVM_DIR", "SCHEDULER_MODE", "LLVM_INSTALL"])

llvm_config.use_default_substitutions()

# excludes: A list of directories to exclude from the testsuite. The 'Inputs'
# subdirectories contain auxiliary inputs for various tests in their parent
# directories.
config.excludes = ["Inputs", "Examples",
                   "CMakeLists.txt", "README.txt", "LICENSE.txt"]

# test_exec_root: The root path where tests should be run.
config.test_exec_root = os.path.join(config.iara_obj_root, "test")
config.iara_tools_dir = os.path.join(config.iara_obj_root, "bin")
config.iara_libs_dir = os.path.join(config.iara_obj_root, "lib")

config.substitutions.append(("%iara_libs", config.iara_libs_dir))

# Tweak the PATH to include the tools dir.
# llvm_config.with_environment("PATH", config.llvm_tools_dir, append_path=True)

tool_dirs = [config.iara_tools_dir, config.llvm_tools_dir]
tools = [
    "mlir-opt",
    "iara-opt",
]

llvm_config.add_tool_substitutions(tools, tool_dirs)

llvm_config.with_environment(
    "PYTHONPATH",
    [
        os.path.join(config.mlir_obj_dir, "python_packages", "iara"),
    ],
    append_path=True,
)
