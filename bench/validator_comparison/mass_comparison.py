"""Size a stratified comparison from its pilot and summarize paired game timings."""
import argparse
import csv
import gzip
import json
import math
import random
from collections import Counter, defaultdict
from itertools import chain
from pathlib import Path
from statistics import fmean, quantiles, variance
from typing import NamedTuple

from bench.corpus.sample_turns import BENCH, PILOT, STAGES, write_benchmark

MASS = BENCH / "fixtures/mass/corpus.json.gz"
WORKLOADS = ("first_board", "new_board", "new_rack")
DRAWS = 20000
SEED = 15


class Pair(NamedTuple):
    original: float
    packed: float

    @property
    def saving(self):
        return self.original - self.packed


def stage_mean(groups, field):
    return fmean(fmean(getattr(pair, field) for pair in group) for group in groups)


def precision_target(original_ms):
    return 0.01 * original_ms


def games_per_stage(groups):
    # For m games per stage, Var(equal-stage mean) = sum(stage variances) / (9m).
    variances = [variance(pair.saving for pair in group) for group in groups]
    target = precision_target(stage_mean(groups, "original"))
    return max(2, math.ceil(1.96 ** 2 * sum(variances) / (len(groups) * target) ** 2))


class Estimate(NamedTuple):
    original: float
    packed: float
    saving: float
    low: float
    high: float


def summarize(groups):
    rng = random.Random(SEED)
    # A difference encodes both observations from the same game. Draw the
    # original number of games in each stage, then equally weight stage means.
    savings = [[pair.saving for pair in group] for group in groups]
    draws = [fmean(fmean(rng.choices(stage, k=len(stage))) for stage in savings)
             for _ in range(DRAWS)]
    percentiles = quantiles(draws, n=1000, method="inclusive")
    return Estimate(stage_mean(groups, "original"), stage_mean(groups, "packed"),
                    stage_mean(groups, "saving"), percentiles[24], percentiles[974])


def recovery(first_saving, reuse_saving, interval):
    if first_saving >= 0:
        return "0"
    low, high = interval
    if high < 0:
        return "no recovery"
    if low <= 0:
        return "unresolved"
    return str(math.ceil(-first_saving / reuse_saving))


def read_timings(path, condition, cases):
    expected = [(f"{case['game']}#{case['turn']}", case["stage"], workload)
                for case in cases for workload in WORKLOADS]
    games = {workload: {stage: defaultdict(list) for stage in STAGES} for workload in WORKLOADS}
    with path.open(newline="") as stream:
        for (name, stage, workload), row in zip(expected, csv.DictReader(stream), strict=True):
            if (row["case"], row["stage"], row["workload"], row["cache"], row["repetition"]) != (
                    name, stage, workload, condition, "1"):
                raise ValueError(f"{path}: expected one ordered {condition} {workload} pair for {name}")
            pair = Pair(float(row["original_total_ms"]), float(row["packed_total_ms"]))
            if not all(math.isfinite(ms) and ms > 0 for ms in pair):
                raise ValueError(f"{name}: invalid elapsed times")
            # The sampler exports one board file per game. A companion with a
            # reusable rack can be selected twice when the next rack is a singleton.
            games[workload][stage][row["board"]].append(pair)
    groups = {}
    for workload, stages in games.items():
        groups[workload] = [
            [Pair(fmean(p.original for p in repeats), fmean(p.packed for p in repeats))
             for repeats in by_game.values()]
            for by_game in stages.values()]
    return groups


def select_cases(corpus, per_stage):
    selected = []
    counts = Counter()
    for case in corpus["cases"]:
        stage = case["stage"]
        if counts[stage] < per_stage:
            selected.append(case)
            counts[stage] += 1
    if any(counts[stage] != per_stage for stage in STAGES):
        raise ValueError(f"need {per_stage} distinct verified games per stage, found {dict(counts)}")
    return selected


def report(groups):
    print("| Workload | Stage | Games | Original ms | Packed ms | Saving ms | Game saving range ms | 95% CI on saving, ms |")
    print("| --- | --- | ---: | ---: | ---: | ---: | --- | --- |")
    estimates = {}
    for workload, stages in groups.items():
        result = estimates[workload] = summarize(stages)
        for stage, pairs in zip(STAGES, stages, strict=True):
            savings = [p.saving for p in pairs]
            print(f"| {workload} | {stage} | {len(pairs)} | {fmean(p.original for p in pairs):.6f} "
                  f"| {fmean(p.packed for p in pairs):.6f} | {fmean(savings):+.6f} "
                  f"| {min(savings):+.6f} to {max(savings):+.6f} | |")
        pairs = list(chain.from_iterable(stages))
        savings = [p.saving for p in pairs]
        print(f"| {workload} | Equal-stage mean | {len(pairs)} | {result.original:.6f} "
              f"| {result.packed:.6f} | {result.saving:+.6f} | {min(savings):+.6f} to {max(savings):+.6f} "
              f"| [{result.low:+.6f}, {result.high:+.6f}] |")
    first = estimates["first_board"]
    target = precision_target(first.original)
    half_width = (first.high - first.low) / 2
    print(f"\nFirst-board half-width {half_width:.6f} ms, target {target:.6f} ms "
          f"({'met' if half_width <= target else 'missed'}). "
          f"The interval {'includes' if first.low <= 0 <= first.high else 'excludes'} zero.")
    for workload in ("new_board", "new_rack"):
        reuse = estimates[workload]
        count = recovery(first.saving, reuse.saving, (reuse.low, reuse.high))
        print(f"\nAdditional calls for {workload} at the observed first-board means: {count}.")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--pilot", type=Path, required=True, help="paired CSV for the six balanced pilot games")
    parser.add_argument("--condition", choices=("hit", "miss"), required=True)
    parser.add_argument("--corpus", type=Path, default=MASS, help="verified mass corpus (.json or .json.gz)")
    action = parser.add_mutually_exclusive_group(required=True)
    action.add_argument("--benchmark-dir", type=Path, help="export the planned mass sample for the paired runner")
    action.add_argument("--report", type=Path, help="report the completed paired CSV for the planned sample")
    args = parser.parse_args()

    pilot = json.loads((PILOT / "corpus.json").read_text())["cases"]
    groups = read_timings(args.pilot, args.condition, pilot)["first_board"]
    size = games_per_stage(groups)
    with (gzip.open if args.corpus.suffix == ".gz" else open)(args.corpus, "rt") as stream:
        cases = select_cases(json.load(stream), size)
    if {c["dictionary_sha256"] for c in cases} != {c["dictionary_sha256"] for c in pilot}:
        parser.error("pilot and mass corpus dictionary fingerprints differ")
    if {c["source"] for c in cases} & {c["source"] for c in pilot}:
        parser.error("pilot and mass comparison must use distinct games")
    print(f"Pilot {args.condition} target {precision_target(stage_mean(groups, 'original')):.6f} ms. "
          f"Stage SDs of first-board savings (ms): "
          + ", ".join(f"{math.sqrt(variance(p.saving for p in g)):.6f}" for g in groups)
          + f". Planned {size} games per stage, {3 * size} total.\n")
    if args.benchmark_dir:
        write_benchmark(args.benchmark_dir, cases)
    else:
        report(read_timings(args.report, args.condition, cases))


if __name__ == "__main__":
    main()
