"""Build a measurement worker with the solver and driver belonging to its Git revision."""
import json
import subprocess
from pathlib import Path

from .benchmark_inputs import ROOT, sha256

BASELINE = "2dde5e1500ac6ce61382359c2b88f6b16a212eef"
FLAGS = ["-std=c++17", "-O2", "-march=x86-64", "-Iinclude", "-Isrc", "-Ibench/measurement_support"]


def git(*args):
    return subprocess.check_output(["git", *args], cwd=ROOT, text=True).strip()


def build_worker(target, revision, name):
    revision = git("rev-parse", "--verify", revision + "^{commit}")
    driver = f"bench/selective_anagram/{name}.cpp"
    paths = ["src", "include", "bench/measurement_support"]
    current_api = bool(git("ls-tree", "--name-only", revision, "include/solver.h"))
    if current_api:
        paths.append(driver)
        if name == "qualification":
            paths.append("tests")
    target.mkdir()
    archive = target / "source.tar"
    subprocess.run(["git", "archive", "--output", archive, revision, *paths], cwd=ROOT, check=True)
    subprocess.run(["tar", "-xf", archive, "-C", target], check=True)
    flags = list(FLAGS)
    if current_api:
        sources = [str(p.relative_to(target)) for p in sorted((target / "src").glob("*.cpp"))
                   if p.name != "main.cpp"]
        if name == "qualification":
            flags.extend(["-Itests", "-DSCRABBLE_TRACE_PLACEMENTS", "-UNDEBUG"])
            sources.append("tests/placement_trace.cpp")
    else:
        # Reuse the recorded build inputs and matching driver for pre-refactor measurements.
        # The reference cache reader predates the driver, so take it from the release checkout.
        manifest = json.loads(git("show", f"{BASELINE}:bench/results/selective-anagram/timing/manifest.json"))
        sources = [arg for arg in manifest["builds"]["B"]["command"] if arg.startswith("src/")]
        source = subprocess.check_output(["git", "show", f"{BASELINE}:{driver}"], cwd=ROOT)
        driver = f"{name}.cpp"
        (target / driver).write_bytes(source)
    command = ["clang++", *flags, driver, *sources, "-o", "worker"]
    subprocess.run(command, cwd=target, check=True)
    return dict(revision=revision, command=command, driver_sha256=sha256(target / driver),
                binary_sha256=sha256(target / "worker"))
