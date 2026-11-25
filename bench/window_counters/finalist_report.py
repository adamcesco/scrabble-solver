"""Verify the pair schedule and report stage means, paired game intervals and development selections."""
import argparse
import json
from collections import defaultdict
from pathlib import Path
from statistics import fmean

from bench.corpus.sample_turns import STAGES
from bench.window_counters.compiler_trials import pair_filename
from bench.window_counters.finalist_trials import CONTENDERS
from bench.window_counters.tuning_report import Game, estimate, read_pairs

ARCHIVE = Path("bench/results/window-finalists")


def report(directory):
    manifest = json.loads((directory / "manifest.json").read_text())
    files = directory / "pairs" if (directory / "pairs").is_dir() else directory
    positions = {split: [Game(n, manifest["inputs"][n]["stage"]) for n in names]
                 for split, names in manifest["case_order"].items()}
    pilot = {case.name for case in positions.get("pilot", [])}
    dataset = "development" if pilot else "held-out"
    alignment = manifest.get("study") == "alignment"
    for target in manifest["builds"]:
        means, intervals = defaultdict(list), {}
        print(f"\n## {target}\n")
        separate_header = " Outside pilot saving (95% CI) |" if pilot else ""
        print("| Left | Right | Early L/R | Middle L/R | Late L/R | Equal-stage L/R | Saving (95% CI) | Outcome |"
              + separate_header)
        print("| --- | --- | ---: | ---: | ---: | ---: | ---: | --- |" + (" ---: |" if pilot else ""))
        for left, right in manifest["pairs"]:
            if left[0] != target:
                continue
            a, b = left[1], right[1]
            if pilot:
                read_pairs(files / pair_filename("pilot", left, right), positions["pilot"], manifest["schedule"]["pilot"],
                           manifest["counts"])
            games = read_pairs(files / pair_filename(dataset, left, right), positions[dataset],
                               manifest["schedule"][dataset], manifest["counts"])
            left_mean, right_mean, saving, low, high = estimate(games, set())
            stages = []
            for stage in STAGES:
                left_games, right_games = zip(*(p for (s, _), p in games.items() if s == stage))
                stages.append(f"{fmean(left_games):.2f} / {fmean(right_games):.2f}")
            outcome = "right faster" if low > 0 else "right slower" if high < 0 else "tie"
            separate = ""
            if pilot:
                rest = estimate(games, pilot)
                separate = f" {rest.saving:+.2f} ({rest.low:+.2f}, {rest.high:+.2f}) |"
            labels = (".".join(left), ".".join(right)) if left[0] != right[0] else (a, b)
            print(f"| {labels[0]} | {labels[1]} | {' | '.join(stages)} | {left_mean:.2f} / {right_mean:.2f} "
                  f"| {saving:+.2f} ({low:+.2f}, {high:+.2f}) | {outcome} |" + separate)
            if pilot and not alignment and a in CONTENDERS and b in CONTENDERS:
                means[a].append(left_mean)
                means[b].append(right_mean)
                intervals[a, b] = (low, high)
                intervals[b, a] = (-high, -low)
        if means:
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
