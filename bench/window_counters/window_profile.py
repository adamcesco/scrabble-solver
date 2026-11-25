"""Qualify the named counters and report their untimed workload on development games."""
import argparse
import csv
import json
import sys
import tempfile
from collections import Counter, defaultdict
from itertools import groupby
from operator import itemgetter
from pathlib import Path

from bench.corpus.sample_turns import STAGES, write_benchmark
from bench.measurement_support.benchmark_inputs import ROOT, Position, qualify_positions, read_manifest, select_cache, sha256
from bench.window_counters.window_benchmark import window_build
from bench.window_counters.window_games import reserved_cases


def window_histograms():
    return dict(windows=0, by_empty_squares=Counter(), by_constrained_gaps=Counter(), by_candidates=Counter())


def variant_profile():
    return dict(windows={kind: window_histograms() for kind in ("no_dictionary_match", "no_matching_group", "matching_groups")},
                groups_by_size=defaultdict(Counter), one_gap_groups=0, one_gap_candidates=0)


def summarize(visits, cases, variants, qualified):
    stages = {stage: dict(games=0, qualified_candidates=Counter(),
                          variants={name: variant_profile() for name in variants}) for stage in STAGES}
    observed = Counter()
    window_key = itemgetter("position", "variant", "down", "empty", "constrained", "pattern_match", "row", "start", "length")
    for (index, name, down, empty, constrained, pattern, *_), groups in groupby(csv.DictReader(visits), window_key):
        case = cases[int(index)]
        profile = stages[case["stage"]]["variants"][name]
        candidates = 0
        for row in groups:
            size = int(row["group_size"])
            if size:
                bucket = profile["groups_by_size"][size]
                bucket["groups"] += 1
                bucket["candidate_words"] += size
                bucket["individual_checks"] += int(row["individual_checks"])
                candidates += size
                if empty == "1":
                    profile["one_gap_groups"] += 1
                    profile["one_gap_candidates"] += size

        kind = "no_matching_group" if pattern == "1" else "no_dictionary_match"
        if candidates:
            kind = "matching_groups"
        windows = profile["windows"][kind]
        windows["windows"] += 1
        windows["by_empty_squares"][int(empty)] += 1
        windows["by_constrained_gaps"][int(constrained)] += 1
        windows["by_candidates"][candidates] += 1
        observed[int(index), name, int(down)] += candidates

    for index, (case, counts) in enumerate(zip(cases, qualified, strict=True)):
        stage = stages[case["stage"]]
        stage["games"] += 1
        for down, direction in enumerate(("across", "down")):
            expected = counts[direction + "_candidates"]
            for name in variants:
                if observed[index, name, down] != expected:
                    raise ValueError(f"{case['case']} {name} {direction}: group sizes differ from {expected} qualified candidates")
            stage["qualified_candidates"][direction] += expected
    return stages


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--variants", nargs="+", default=["packed", "one-gap"])
    parser.add_argument("--dictionary", type=Path, default=ROOT / "data/dictionaries/sample_dictionary.txt")
    args = parser.parse_args()
    if len(set(args.variants)) != len(args.variants):
        parser.error("expected distinct variant names")
    development = reserved_cases("development")
    fingerprint = sha256(args.dictionary)
    if fingerprint != development[0]["dictionary_sha256"]:
        parser.error("dictionary differs from the recorded development corpus")

    build = window_build()
    with tempfile.TemporaryDirectory() as tmp:
        directory = Path(tmp)
        write_benchmark(directory, development, paired=False)
        manifest = directory / "cases.csv"
        cases = read_manifest(manifest)
        positions = [Position.from_row(manifest, row) for row in cases]
        cache = select_cache(build / "window_worker", args.dictionary, directory / "patterns")
        visits = directory / "visits.csv"
        counts = qualify_positions(args.dictionary, cache, positions, args.variants, build, profile=visits)
        with visits.open(newline="") as stream:
            stages = summarize(stream, cases, args.variants, (counts[pos] for pos in positions))
    json.dump(dict(game_set="development", dictionary_sha256=fingerprint, stages=stages),
              sys.stdout, sort_keys=True, indent=2)
    print()


if __name__ == "__main__":
    main()
