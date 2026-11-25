"""Qualify two window counters, then time paired solves on each manifest's primary board and rack."""
import argparse
import csv
import io
import os
import platform
import subprocess
import sys
import tempfile
from collections import Counter
from pathlib import Path

from paired_benchmark import ROOT, FIXTURE, qualify_positions, read_cases, select_cache, sha256


def window_build():
    directory = subprocess.check_output(["make", "-s", "--no-print-directory", "window-tools"], cwd=ROOT, text=True)
    return ROOT / directory.strip()


def paired_order(cases, repetitions):
    for repetition in range(repetitions):
        stages = Counter()
        for case in cases:
            first = (repetition + stages[case.stage]) % 2
            stages[case.stage] += 1
            yield repetition + 1, case, first


def build_info(worker, dictionary):
    compiler, flags = subprocess.check_output([worker, "--build-info"], text=True).splitlines()
    cpu = platform.processor() or platform.machine()
    cpuinfo = Path("/proc/cpuinfo")
    if cpuinfo.is_file():
        cpu = next((line.partition(":")[2].strip() for line in cpuinfo.read_text().splitlines()
                    if line.startswith("model name")), cpu)
    affinity = ",".join(map(str, sorted(os.sched_getaffinity(0)))) if hasattr(os, "sched_getaffinity") else "inherited"
    return dict(compiler=compiler, flags=flags, worker_sha256=sha256(worker), cpu=cpu, affinity=affinity,
                dictionary=os.path.relpath(dictionary), dictionary_sha256=sha256(dictionary))


def paired_rows(worker, dictionary, cache, cases, variants, repetitions, counts):
    info = build_info(worker, dictionary)
    board_hashes = {p.board: sha256(p.board) for p in counts}
    schedule = list(paired_order(cases, repetitions))
    requests = "".join(f"{case.first.board}\t{case.first.rack}\t{first}\n" for _, case, first in schedule)
    output = subprocess.check_output([worker, *variants, dictionary, cache], input=requests, text=True)
    observations = csv.reader(io.StringIO(output))
    for repetition, case, first in schedule:
        position = case.first
        expected = counts[position]
        times = {}
        for index in (first, 1 - first):
            name, across, down, solve_us = next(observations)
            if name != variants[index] or (int(across), int(down)) != (expected["across"], expected["down"]):
                raise ValueError(f"{case.name}: {name} release observation differs from qualification")
            times[name] = float(solve_us)
        baseline, challenger = variants
        yield dict(case=case.name, stage=case.stage, repetition=repetition, board=os.path.relpath(position.board),
                   board_sha256=board_hashes[position.board], rack=position.rack, baseline=baseline,
                   challenger=challenger, first_variant=variants[first], **expected,
                   baseline_solve_us=times[baseline], challenger_solve_us=times[challenger],
                   saving_us=f"{times[baseline] - times[challenger]:.3f}", **info)
    if next(observations, None) is not None:
        raise ValueError("unexpected extra release observations")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--variants", nargs=2, metavar=("BASELINE", "CHALLENGER"), default=("packed", "one-gap"))
    parser.add_argument("--dictionary", type=Path, default=FIXTURE / "dictionary.txt")
    parser.add_argument("--cases", type=Path, default=FIXTURE / "cases.csv")
    parser.add_argument("--cache", type=Path, help="existing matching pattern cache")
    parser.add_argument("--repetitions", type=int, default=4)
    parser.add_argument("--build-dir", type=Path, default=os.environ.get("WINDOW_BUILD"), help=argparse.SUPPRESS)
    args = parser.parse_args()
    if args.repetitions < 1 or len(set(args.variants)) != 2:
        parser.error("expected positive repetitions and two distinct variant names")
    if args.cache is not None and not args.cache.is_file():
        parser.error(f"pattern cache missing: {args.cache}")
    cases = read_cases(args.cases)
    build = args.build_dir or window_build()
    worker = build / "window_worker"
    with tempfile.TemporaryDirectory() as directory:
        cache = select_cache(worker, args.dictionary, args.cache or Path(directory) / "patterns")
        positions = (case.first for case in cases)
        counts = qualify_positions(args.dictionary, cache, positions, args.variants, build)
        rows = list(paired_rows(worker, args.dictionary, cache.path, cases, args.variants, args.repetitions, counts))
        writer = csv.DictWriter(sys.stdout, fieldnames=rows[0], lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)


if __name__ == "__main__":
    main()
