"""Measure paired prepared solves on the frozen held-out games, or reanalyze the observations."""

import argparse
import csv
import fcntl
import gzip
import json
import os
import platform
import random
import subprocess
import sys
import tempfile
from collections import Counter, defaultdict
from contextlib import ExitStack, contextmanager
from pathlib import Path
from statistics import fmean

from bench.measurement_support.benchmark_inputs import sha256
from bench.measurement_support.paired_timings import DRAWS, SEED, Pair, summarize
from bench.measurement_support.rack_neighbor_build import SOURCE_DIRS, build_worker
from bench.selective_anagram.reference import CACHE, DICTIONARY, REVISION, ROOT

STAGES = ("early", "middle", "late")
REPETITIONS = 40
MANIFEST_SHA256 = "0349591e1aad857ed255c82741cd1b508d21d7f276c896fe94d37b92368afcad"
FIELDS = ("case", "stage", "repetition", "order", "baseline_ns", "production_ns",
          "baseline_across", "baseline_down", "production_across", "production_down")


def held_out(reference):
    manifest = reference / "cases.csv"
    if sha256(manifest) != MANIFEST_SHA256:
        raise ValueError("cases.csv differs from the frozen four-way manifest")
    with manifest.open() as stream:
        cases = [c for c in csv.DictReader(stream) if c["cohort"] == "held-out"]
    if len({c["case"] for c in cases}) != 96 or Counter(c["stage"] for c in cases) != dict.fromkeys(STAGES, 32):
        raise ValueError("expected 32 distinct held-out games from each stage")
    return cases


def schedule(size):
    rng = random.Random(15)
    for rep in range(REPETITIONS):
        for i in rng.sample(range(size), size):
            yield i, rep + 1, ("AB", "BA")[(i + rep) % 2]


def report(reference, directory):
    cases = held_out(reference)
    games = defaultdict(list)
    with gzip.open(directory / "blocks.csv.gz", "rt") as stream:
        for (i, rep, order), row in zip(schedule(len(cases)), csv.DictReader(stream), strict=True):
            case = cases[i]
            if (row["case"], row["stage"], int(row["repetition"]), row["order"]) != (
                    case["case"], case["stage"], rep, order):
                raise ValueError(f"{case['case']}: expected repetition {rep}, order {order}")
            for solver in ("baseline", "production"):
                if any(row[f"{solver}_{d}"] != case[d] for d in ("across", "down")):
                    raise ValueError(f"{case['case']}: {solver} directional counts differ from archive")
            sample = Pair(int(row["baseline_ns"]) / 1000, int(row["production_ns"]) / 1000)
            if min(sample) <= 0:
                raise ValueError(f"{case['case']}: elapsed time must be positive")
            games[case["stage"], case["case"]].append(sample)
    groups = []
    for stage in STAGES:
        group = []
        for (s, _), pairs in sorted(games.items()):
            if s == stage:
                group.append(Pair(fmean(p.original for p in pairs), fmean(p.packed for p in pairs)))
        groups.append(group)
    result = summarize(groups)
    summary = dict(games=len(games), pairs=len(games) * REPETITIONS, baseline_us=result.original,
                   production_us=result.packed, saving_us=result.saving, low95_us=result.low,
                   high95_us=result.high, passed=result.low > 0)
    print(json.dumps(summary, indent=2))
    return summary


HERE = Path(__file__).resolve().parent
DICT_SHA256 = "559faa1e0296166f8f4c6074b71211c045202accb6920424aaa0758cc7bce9cd"


def git(*args):
    return subprocess.check_output(["git", *args], cwd=ROOT, text=True).strip()


def build(revision, target):
    revision = git("rev-parse", "--verify", revision + "^{commit}")
    command = build_worker(target, revision, HERE / "timing.cpp")
    trees = {path: git("rev-parse", f"{revision}:{path}") for path in SOURCE_DIRS}
    return dict(revision=revision, source_trees=trees, binary_sha256=sha256(target / "worker"),
                command=command)


@contextmanager
def prepared_worker(executable, cpu, data, reference, cases):
    cmd = ["taskset", "-c", str(cpu), executable, data / DICTIONARY, data / CACHE]
    process = subprocess.Popen(cmd, cwd=reference / "inputs", stdin=subprocess.PIPE,
                               stdout=subprocess.PIPE, text=True)
    try:
        process.stdin.write(str(len(cases)) + "\n")
        for case in cases:
            process.stdin.write(f"{case['board']} {case['rack']}\n")
        process.stdin.flush()
        if process.stdout.readline().split() != ["ready", str(len(cases))]:
            raise ValueError(f"{executable}: worker failed to prepare positions")
        if os.sched_getaffinity(process.pid) != {cpu}:
            raise ValueError(f"{executable}: worker requires affinity to logical CPU {cpu}")
        yield process
        process.stdin.close()
        if code := process.wait():
            raise RuntimeError(f"{executable}: worker exited with {code}")
    finally:
        if process.poll() is None:
            process.kill()
        process.wait()
        process.stdout.close()
        process.stdin.close()


def solve(process, i, case):
    process.stdin.write(f"{i}\n")
    process.stdin.flush()
    response = process.stdout.readline().split()
    if len(response) != 4 or response[0] != str(i) or response[2:] != [case["across"], case["down"]]:
        raise ValueError(f"{case['case']}: expected case {i}, {case['across']} across, {case['down']} down; got {response}")
    ns = int(response[1])
    if ns <= 0:
        raise ValueError(f"{case['case']}: expected positive elapsed time")
    return ns, *map(int, response[2:])


def paging(workers):
    counters = {}
    for name, process in workers.items():
        # Field 12 of /proc/PID/stat, after the parenthesized command (field 2).
        fields = Path(f"/proc/{process.pid}/stat").read_text().rsplit(")", 1)[1].split()
        counters[name + "_major_faults"] = int(fields[9])
    for line in Path("/proc/vmstat").read_text().splitlines():
        field, value = line.split()
        if field in ("pswpin", "pswpout"):
            counters[field] = int(value)
    return counters


def run_pairs(workers, cases, directory):
    preflight = [paging(workers)]
    clean = 0
    while clean < 2:
        for i, case in enumerate(cases):
            for process in workers.values():
                solve(process, i, case)
        before = preflight[-1]
        after = paging(workers)
        preflight.append(after)
        if all(before[k] == after[k] for k in ("A_major_faults", "B_major_faults")):
            clean += 1
        else:
            clean = 0
        if len(preflight) == 101 and clean < 2:
            raise RuntimeError("both full tables could not sustain two fault-free held-out sweeps")

    rows = []
    before = paging(workers)
    for i, rep, order in schedule(len(cases)):
        case = cases[i]
        for _ in range(2):  # Same AB/BA order in each warm-up round and the measured round.
            for name in order:
                solve(workers[name], i, case)
        measured = {name: solve(workers[name], i, case) for name in order}
        a, b = measured["A"], measured["B"]
        rows.append((case["case"], case["stage"], rep, order, a[0], b[0], *a[1:], *b[1:]))
    after = paging(workers)
    with gzip.open(directory / "blocks.csv.gz", "wt") as stream:
        writer = csv.writer(stream, lineterminator="\n")
        writer.writerow(FIELDS)
        writer.writerows(rows)
    return dict(preflight=preflight, before_blocks=before, after_blocks=after)


def write_json(path, value):
    path.write_text(json.dumps(value, indent=2) + "\n")


def measure(data, reference, directory, production, cpu):
    data, reference, directory = data.resolve(), reference.resolve(), directory.resolve()
    cases = held_out(reference)
    for case in cases:
        if sha256(reference / "inputs" / case["board"]) != case["board_sha256"]:
            raise ValueError(f"{case['case']}: board digest differs from archive")
    if sha256(data / DICTIONARY) != DICT_SHA256:
        raise ValueError("dictionary digest differs from archive")
    directory.mkdir(parents=True, exist_ok=False)
    with tempfile.TemporaryDirectory() as tmp:
        temp = Path(tmp)
        builds = {"A": build(REVISION, temp / "A"), "B": build(production, temp / "B")}
        model = next(line.split(":", 1)[1].strip() for line in Path("/proc/cpuinfo").read_text().splitlines()
                     if line.startswith("model name"))
        manifest = dict(builds=builds, compiler=subprocess.check_output(["clang++", "--version"], text=True),
                        driver_sha256=sha256(HERE / "timing.cpp"), coordinator_sha256=sha256(Path(__file__)),
                        kernel=platform.release(), python=platform.python_version(), cpu_model=model,
                        worker_cpu=cpu, coordinator_cpu_affinity=sorted(os.sched_getaffinity(0)),
                        cases_sha256=MANIFEST_SHA256, dictionary_sha256=DICT_SHA256,
                        shuffle_seed=15, warmup_rounds=2, repetitions=REPETITIONS,
                        bootstrap_draws=DRAWS, bootstrap_seed=SEED)
        # All full-table users hold this same lock until their workers have exited.
        with open("/tmp/selective-anagram-full-run.lock", "a") as lock:
            print("Waiting for the shared full-run lock", file=sys.stderr, flush=True)
            fcntl.flock(lock, fcntl.LOCK_EX)
            print("Hashing the shared v3 cache", file=sys.stderr, flush=True)
            manifest["cache_bytes"] = (data / CACHE).stat().st_size
            manifest["cache_sha256"] = sha256(data / CACHE)
            with ExitStack() as stack:
                workers = {}
                for label in builds:
                    print(f"Preparing worker {label}", file=sys.stderr, flush=True)
                    workers[label] = stack.enter_context(prepared_worker(temp / label / "worker", cpu, data,
                                                                          reference, cases))
                print("Running preflight and paired blocks", file=sys.stderr, flush=True)
                manifest["paging"] = run_pairs(workers, cases, directory)
        write_json(directory / "manifest.json", manifest)
        write_json(directory / "summary.json", report(reference, directory))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    modes = parser.add_subparsers(dest="mode", required=True)
    analyze = modes.add_parser("report", help="validate the raw block schedule and print the release estimate")
    analyze.add_argument("reference", type=Path)
    analyze.add_argument("directory", type=Path)
    run = modes.add_parser("measure", help="build the two revisions and run the complete qualification")
    run.add_argument("data", type=Path)
    run.add_argument("reference", type=Path)
    run.add_argument("directory", type=Path, help="new output directory")
    run.add_argument("--production", required=True, help="git revision of the production solver")
    run.add_argument("--cpu", type=int, default=2)
    args = parser.parse_args()
    if args.mode == "measure":
        measure(args.data, args.reference, args.directory, args.production, args.cpu)
    else:
        report(args.reference, args.directory)


if __name__ == "__main__":
    main()
