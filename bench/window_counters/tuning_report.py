"""Verify the saved pilot and development pairs and print the development estimates."""
import argparse
import csv
import gzip
import json
from collections import defaultdict
from pathlib import Path
from statistics import fmean
from typing import NamedTuple

from bench.corpus.sample_turns import STAGES
from bench.measurement_support.paired_timings import Pair, summarize
from bench.window_counters.compiler_trials import comparisons, pair_filename
from bench.window_counters.window_benchmark import paired_order

ARCHIVE = Path("bench/results/window-compiler")


class Game(NamedTuple):
    name: str
    stage: str


def read_pairs(path, cases, reps, counts):
    games = defaultdict(list)
    with gzip.open(path, "rt") as stream:
        for row, (rep, case, first) in zip(csv.DictReader(stream), paired_order(cases, reps), strict=True):
            observed = (row["case"], row["stage"], int(row["repetition"]), int(row["first"]),
                        int(row["across"]), int(row["down"]))
            expected = counts[case.name]
            if observed != (case.name, case.stage, rep, first, expected["across"], expected["down"]):
                raise ValueError(f"{path}: mismatched observation at {case.name} repetition {rep}")
            games[case.stage, case.name].append(Pair(float(row["left_us"]), float(row["right_us"])))
    return {game: Pair(*map(fmean, zip(*pairs))) for game, pairs in games.items()}


def estimate(games, excluded):
    return summarize([[pair for (phase, name), pair in sorted(games.items()) if phase == stage and name not in excluded]
                      for stage in STAGES])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", nargs="?", type=Path, default=ARCHIVE)
    directory = parser.parse_args().directory
    pairs = directory / "pairs" if (directory / "pairs").is_dir() else directory
    manifest = json.loads((directory / "manifest.json").read_text())
    cases = {split: [Game(name, manifest["inputs"][name]["stage"]) for name in names]
             for split, names in manifest["case_order"].items()}
    pilot = set(manifest["case_order"]["pilot"])
    print("| Left build · counter | Right build · counter | Left / right µs | Saving µs (95% CI) "
          "| Saving outside pilot (95% CI) |")
    print("| --- | --- | ---: | ---: | ---: |")
    for left, right in comparisons(manifest["builds"]):
        a, b = (".".join(side) for side in (left, right))
        for dataset in ("pilot", "development"):
            path = pairs / pair_filename(dataset, left, right)
            games = read_pairs(path, cases[dataset], manifest["schedule"][dataset], manifest["counts"])
        full = estimate(games, set())
        separate = estimate(games, pilot)
        print(f"| {a} | {b} | {full.original:.2f} / {full.packed:.2f} "
              f"| {full.saving:+.2f} ({full.low:+.2f}, {full.high:+.2f}) "
              f"| {separate.saving:+.2f} ({separate.low:+.2f}, {separate.high:+.2f}) |")


if __name__ == "__main__":
    main()
