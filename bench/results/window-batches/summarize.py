"""Print paired µs/solve and stratified game-level intervals for the development CSV."""
import argparse
import csv
import gzip
import json
from collections import defaultdict
from pathlib import Path
from statistics import fmean

from bench.corpus.sample_turns import PILOT, STAGES, case_name
from bench.measurement_support.paired_timings import Pair, summarize
from bench.window_counters.window_games import reserved_cases


def estimate(by_game, excluded=()):
    stages = []
    for stage in STAGES:
        games = []
        for (phase, case), pairs in sorted(by_game.items()):
            if phase == stage and case not in excluded:
                baseline_times, challenger_times = zip(*pairs)
                games.append(Pair(fmean(baseline_times), fmean(challenger_times)))
        stages.append(games)
    return summarize(stages)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("timings", nargs="*", type=Path, default=[Path(__file__).with_name("development.csv.gz")])
    paths = parser.parse_args().timings
    trials = defaultdict(lambda: defaultdict(list))
    for path in paths:
        open_csv = gzip.open if path.suffix == ".gz" else open
        with open_csv(path, "rt", newline="") as stream:
            for row in csv.DictReader(stream):
                games = trials[row["baseline"], row["challenger"]]
                games[row["stage"], row["case"]].append(
                    Pair(float(row["baseline_solve_us"]), float(row["challenger_solve_us"])))
    pilot = {case_name(case) for case in json.loads((PILOT / "corpus.json").read_text())["cases"]}
    development = {(case["stage"], case_name(case)) for case in reserved_cases("development")}
    print("| Control | Challenger | Control / challenger | Saving (95% CI) | Saving outside pilot (95% CI) |")
    print("| --- | --- | ---: | ---: | ---: |")
    for (base, challenger), games in trials.items():
        if games.keys() != development:
            parser.error(f"{base} / {challenger} needs one game mean for every development case")
        baseline_time, challenger_time, saving, low, high = estimate(games)
        separate = estimate(games, pilot)
        print(f"| `{base}` | `{challenger}` | {baseline_time:.2f} / {challenger_time:.2f} "
              f"| {saving:+.2f} ({low:+.2f} to {high:+.2f}) "
              f"| {separate.saving:+.2f} ({separate.low:+.2f} to {separate.high:+.2f}) |")


if __name__ == "__main__":
    main()
