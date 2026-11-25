"""Run the development comparisons, or replay the comparison specified by a finalist freeze."""
import argparse
import fcntl
import json
import os
import subprocess
from itertools import combinations
from pathlib import Path

from bench.corpus.sample_turns import PILOT, case_name
from bench.measurement_support.benchmark_inputs import sha256
from bench.window_counters.compiler_trials import CPU, DICTIONARY, input_set, qualification, time_pairs
from bench.window_counters.tuning_builds import PORTABLE, WORK, build, identity, write_json
from bench.window_counters.window_games import GAME_LIST

RESULTS = Path("build/window-finalists")
TARGETS = {"portable": ("-O3", "-march=x86-64"),
           "skylake": ("-O3", "-march=skylake", "-mbranches-within-32B-boundaries")}
CONTENDERS = ("packed", "rack", "neighbor-cache", "rack-cache", "rack-neighbor", "rack-neighbor-cache",
              "rack-all-checks")
# The round robin includes packed, rack, and every two-way constituent of the three-way combination.
CONSTITUENTS = [("row-cache", "neighbor-cache"), ("neighbor-bits", "neighbor-cache"),
                ("row-cache", "rack-cache"), ("neighbor-bits", "rack-neighbor"), ("all-checks", "rack-all-checks")]


def run(frozen):
    os.sched_setaffinity(0, {CPU})
    dataset = "held-out" if frozen else "development"
    inputs = [DICTIONARY, GAME_LIST, PILOT / "corpus.json", Path("bench/fixtures/mass/corpus.json.gz")]
    fingerprints = {os.path.relpath(p): sha256(p) for p in inputs}
    if frozen and fingerprints != frozen["input_sha256"]:
        raise ValueError("dictionary or reserved inputs differ from the freeze")
    cases, members = input_set(dataset, RESULTS)
    host = build("host", PORTABLE)
    if frozen and host["sha256"] != frozen["host_sha256"]:
        raise ValueError("timing host differs from the freeze")
    builds, qualified, pairs = {}, {}, []
    counts = None
    for target, flags in TARGETS.items():
        if frozen:
            selection = frozen["targets"][target]
            counters = ["packed", *selection["finalists"]]
            edges = list(combinations(counters, 2))
        else:
            edges = [*combinations(CONTENDERS, 2), *CONSTITUENTS]
        variants = sorted({name for edge in edges for name in edge})
        builds[target] = build(target, flags, full_registry=True)
        if frozen and builds[target]["sha256"] != selection["binary_sha256"]:
            raise ValueError(f"{target} release build differs from the freeze")
        qualifier_flags = " ".join(["-std=c++17", *flags, "-Wall", "-Wextra", "-Iinclude"])
        tools = Path(subprocess.check_output(["make", "-s", "--no-print-directory", "window-tools",
                                              "CXX=clang++-18", "CXXFLAGS=" + qualifier_flags], text=True).strip())
        cache, reference, qualified[target] = qualification(cases, members, tools, RESULTS, variants=variants)
        if counts is not None and reference != counts:
            raise ValueError(f"{target} reference counts differ from the other qualifier")
        counts = reference
        pairs.extend(((target, a), (target, b)) for a, b in edges)

    schedule = {dataset: 40}
    positions = {dataset: cases}
    if not frozen:
        pilot = {case_name(c) for c in json.loads((PILOT / "corpus.json").read_text())["cases"]}
        schedule = {"pilot": 4, **schedule}
        positions["pilot"] = [c for c in cases if c.name in pilot]
    protocol = {str(path): sha256(path) for path in map(Path, (
        "bench/window_counters/finalist_trials.py", "bench/window_counters/compiler_trials.py",
        "bench/window_counters/window_benchmark.py", "bench/window_counters/tuning_report.py",
        "bench/window_counters/finalist_report.py", "bench/measurement_support/paired_timings.py"))}
    if frozen and protocol != frozen["protocol_sha256"]:
        raise ValueError("measurement or analysis code differs from the freeze")
    metadata = dict(builds=builds, host=host, pairs=pairs, inputs=members, input_sha256=fingerprints,
                    qualification_ids=qualified, counts=counts, schedule=schedule, protocol_sha256=protocol,
                    case_order={s: [c.name for c in group] for s, group in positions.items()},
                    cpu=next(line.split(":", 1)[1].strip() for line in Path("/proc/cpuinfo").read_text().splitlines()
                             if line.startswith("model name")), kernel=os.uname().release, affinity=CPU)
    output = RESULTS / dataset / identity(metadata)[:20]
    output.mkdir(parents=True, exist_ok=True)
    write_json(output / "manifest.json", metadata)
    print(f"results {output}", flush=True)
    time_pairs(output, schedule, pairs, positions, builds, host["binary"], cache, counts)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--frozen", type=Path, help="use this committed finalist freeze on the held-out set")
    args = parser.parse_args()
    RESULTS.mkdir(parents=True, exist_ok=True)
    WORK.mkdir(parents=True, exist_ok=True)
    with (WORK / "run.lock").open("w") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        run(json.loads(args.frozen.read_text()) if args.frozen else None)


if __name__ == "__main__":
    main()
