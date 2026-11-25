"""Verify the frozen pair schedule and print stage means, paired intervals and development selections."""
import argparse
import json
from collections import defaultdict
from pathlib import Path
from statistics import fmean

from bench.corpus.sample_turns import STAGES
from bench.window_counters.finalist_trials import CONTENDERS
from bench.window_counters.tuning_report import Game, estimate, read_pairs

ARCHIVE = Path("bench/results/window-finalists")


def report(directory):
    manifest = json.loads((directory / "manifest.json").read_text())
    files = directory / "pairs" if (directory / "pairs").is_dir() else directory
    positions = {split: [Game(n, manifest["inputs"][n]["stage"]) for n in names]
                 for split, names in manifest["case_order"].items()}
    dataset = "development" if "development" in positions else "held-out"
    for target in manifest["builds"]:
        means, intervals = defaultdict(list), {}
        print(f"\n## {target}\n")
        print("| Left | Right | Early L/R | Middle L/R | Late L/R | Equal-stage L/R | Saving (95% CI) | Outcome |")
        print("| --- | --- | ---: | ---: | ---: | ---: | ---: | --- |")
        for (left_build, left), (right_build, right) in manifest["pairs"]:
            if left_build != target:
                continue
            filename = f"{left_build}.{left}_{right_build}.{right}.csv.gz"
            if "pilot" in positions:
                read_pairs(files / ("pilot_" + filename), positions["pilot"], manifest["schedule"]["pilot"],
                           manifest["counts"])
            games = read_pairs(files / (dataset + "_" + filename), positions[dataset], manifest["schedule"][dataset],
                               manifest["counts"])
            paired = estimate(games, set())
            stages = []
            for stage in STAGES:
                group = [p for (s, _), p in games.items() if s == stage]
                stages.append(f"{fmean(p.original for p in group):.2f} / {fmean(p.packed for p in group):.2f}")
            outcome = "right faster" if paired.low > 0 else "right slower" if paired.high < 0 else "tie"
            print(f"| {left} | {right} | {' | '.join(stages)} | {paired.original:.2f} / {paired.packed:.2f} "
                  f"| {paired.saving:+.2f} ({paired.low:+.2f}, {paired.high:+.2f}) | {outcome} |")
            if left in CONTENDERS and right in CONTENDERS:
                means[left].append(paired.original)
                means[right].append(paired.packed)
                intervals[left, right] = (paired.low, paired.high)
                intervals[right, left] = (-paired.high, -paired.low)
        if dataset == "development":
            ranked = sorted(means, key=lambda n: fmean(means[n]))
            leader = ranked[0]
            finalists = [n for i, n in enumerate(ranked) if n != "packed" and
                         (i < 2 or intervals[leader, n][1] >= 0)]
            print("\nRound-robin means (µs/solve), fastest first. " +
                  ", ".join(f"{n} {fmean(means[n]):.2f}" for n in ranked) + ".")
            print("\nAdvance " + ", ".join(finalists) + ". Include packed as the matched baseline.")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", nargs="?", type=Path, default=ARCHIVE)
    report(parser.parse_args().directory)
