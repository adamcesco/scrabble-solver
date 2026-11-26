"""Build rack-neighbor benchmark drivers against one selected solver revision."""

import shutil
import subprocess

from .benchmark_inputs import ROOT

FLAGS = ["-std=c++17", "-O2", "-march=x86-64", "-Iinclude", "-Isrc", "-Ibench/measurement_support"]
SOURCE_DIRS = ["src", "include", "bench/measurement_support"]
SOURCES = [
    "src/board.cpp", "src/perpendicular_checks.cpp", "src/word_table.cpp", "src/word_pattern_table.cpp",
    "src/utils.cpp", "src/solvers/shared/rack_subsets.cpp", "src/solvers/rack_neighbor/solver.cpp",
]


def build_worker(target, revision, driver):
    target.mkdir()
    archive = target / "source.tar"
    subprocess.run(["git", "archive", "--output", archive, revision, *SOURCE_DIRS], cwd=ROOT, check=True)
    subprocess.run(["tar", "-xf", archive, "-C", target], check=True)
    shutil.copyfile(driver, target / driver.name)
    command = ["clang++", *FLAGS, driver.name, *SOURCES, "-o", "worker"]
    subprocess.run(command, cwd=target, check=True)
    return command
