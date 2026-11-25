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
TARGETS["skylake-unaligned"] = ("-O3", "-march=skylake")
CONTENDERS = ("packed", "rack", "neighbor-cache", "rack-cache", "rack-neighbor", "rack-neighbor-cache",
              "rack-all-checks")
# The round robin includes packed, rack, and every two-way constituent of the three-way combination.
CONSTITUENTS = [("row-cache", "neighbor-cache"), ("neighbor-bits", "neighbor-cache"),
                ("row-cache", "rack-cache"), ("neighbor-bits", "rack-neighbor"), ("all-checks", "rack-all-checks")]


def comparison_pairs(frozen, alignment):
    if frozen is not None:
        for finalists in frozen["targets"].values():
            entrants = list(dict.fromkeys((build, "packed") for build, _ in finalists))
            entrants.extend(tuple(side) for side in finalists)
            yield from combinations(entrants, 2)
    elif alignment:
        aligned, plain = "skylake", "skylake-unaligned"
        # Hold each advancing implementation fixed across flags, and directly compare all four builds/counters.
        leaders = ("rack-neighbor", "rack-neighbor-cache")
        yield (aligned, "packed"), (plain, "packed")
        yield from combinations(((build, name) for build in (aligned, plain) for name in leaders), 2)
        for name in leaders:
            yield (plain, "packed"), (plain, name)
            yield (plain, "rack"), (plain, name)
    else:
        for target in ("portable", "skylake"):
            for a, b in [*combinations(CONTENDERS, 2), *CONSTITUENTS]:
                yield (target, a), (target, b)


def protocol_hashes():
    sources = ("window_counters/finalist_trials.py", "window_counters/compiler_trials.py",
               "window_counters/window_benchmark.py", "window_counters/tuning_report.py",
               "window_counters/finalist_report.py", "measurement_support/paired_timings.py",
               "measurement_support/benchmark_inputs.py", "window_counters/window_games.py",
               "corpus/sample_turns.py", "corpus/replay_game.py", "window_counters/tuning_builds.py")
    return {str(path): sha256(path) for path in (Path("bench") / s for s in sources)}


def run(frozen, alignment=False):
    cpu = next(line.split(":", 1)[1].strip() for line in Path("/proc/cpuinfo").read_text().splitlines()
               if line.startswith("model name"))
    os.sched_setaffinity(0, {CPU})
    dataset = "development" if frozen is None else "held-out"
    inputs = [DICTIONARY, GAME_LIST, PILOT / "corpus.json", Path("bench/fixtures/mass/corpus.json.gz")]
    fingerprints = {os.path.relpath(p): sha256(p) for p in inputs}
    protocol = protocol_hashes()
    if frozen is not None:
        if (cpu, CPU) != (frozen["cpu"], frozen["affinity"]):
            raise ValueError("execution CPU or affinity differs from the freeze")
        if fingerprints != frozen["input_sha256"] or protocol != frozen["protocol_sha256"]:
            raise ValueError("inputs or measurement and analysis code differ from the freeze")
    host = build("host", PORTABLE)
    if frozen is not None and host["sha256"] != frozen["host_sha256"]:
        raise ValueError("timing host differs from the freeze")
    pairs = list(comparison_pairs(frozen, alignment))
    by_build = {}
    for pair in pairs:
        for target, variant in pair:
            by_build.setdefault(target, set()).add(variant)
    builds = {}
    for target in by_build:
        flags = TARGETS[target] if frozen is None else frozen["builds"][target]["target_flags"]
        builds[target] = build(target, flags, full_registry=True)
        if frozen is not None and builds[target]["sha256"] != frozen["builds"][target]["binary_sha256"]:
            raise ValueError(f"{target} release build differs from the freeze")

    cases, members = input_set(dataset, RESULTS)
    qualified, counts = {}, None
    for target, variants in by_build.items():
        flags = TARGETS[target] if frozen is None else frozen["builds"][target]["target_flags"]
        qualifier_flags = " ".join(["-std=c++17", *flags, "-Wall", "-Wextra", "-Iinclude"])
        tools = Path(subprocess.check_output(["make", "-s", "--no-print-directory", "window-tools",
                                              "CXX=clang++-18", "CXXFLAGS=" + qualifier_flags], text=True).strip())
        cache, reference, qualified[target] = qualification(cases, members, tools, RESULTS, variants=sorted(variants))
        if counts is not None and reference != counts:
            raise ValueError(f"{target} reference counts differ from the other qualifier")
        counts = reference

    schedule = {dataset: 40 if frozen is None else frozen["held_out"]["repetitions"]}
    positions = {dataset: cases}
    if frozen is None:
        pilot = {case_name(c) for c in json.loads((PILOT / "corpus.json").read_text())["cases"]}
        schedule = {"pilot": 4, **schedule}
        positions["pilot"] = [c for c in cases if c.name in pilot]
    metadata = dict(study="alignment" if alignment else dataset, builds=builds, host=host, pairs=pairs, inputs=members, input_sha256=fingerprints,
                    qualification_ids=qualified, counts=counts, schedule=schedule, protocol_sha256=protocol,
                    case_order={s: [c.name for c in group] for s, group in positions.items()},
                    cpu=cpu, kernel=os.uname().release, affinity=CPU)
    output = RESULTS / dataset / identity(metadata)[:20]
    output.mkdir(parents=True, exist_ok=True)
    write_json(output / "manifest.json", metadata)
    print(f"results {output}", flush=True)
    time_pairs(output, schedule, pairs, positions, builds, host["binary"], cache, counts)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    study = parser.add_mutually_exclusive_group()
    study.add_argument("--alignment", action="store_true", help="compare native -O3 branch alignment on the development finalists")
    study.add_argument("--frozen", type=Path, help="use this committed finalist freeze on the held-out set")
    args = parser.parse_args()
    RESULTS.mkdir(parents=True, exist_ok=True)
    WORK.mkdir(parents=True, exist_ok=True)
    with (WORK / "run.lock").open("w") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        run(json.loads(args.frozen.read_text()) if args.frozen else None, args.alignment)


if __name__ == "__main__":
    main()
