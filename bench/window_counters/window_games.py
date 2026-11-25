"""Reproduce the window experiment game lists or export one list for solving."""
import argparse
import csv
import gzip
import json
import sys
from collections import Counter
from pathlib import Path

from bench.corpus.sample_turns import BENCH, PILOT, STAGES, case_name, write_benchmark

GAME_LIST = BENCH / "fixtures/window-game-sets.csv"
SETS = ("development", "pgo", "held-out")


def game_sets():
    pilot = json.loads((PILOT / "corpus.json").read_text())["cases"]
    with gzip.open(BENCH / "fixtures/mass/corpus.json.gz", "rt") as stream:
        mass = json.load(stream)["cases"]
    sizes = Counter(c["stage"] for c in mass)
    if any(sizes[stage] < 8 + 16 + 32 for stage in STAGES):
        raise ValueError("need 56 separate recorded games in each stage")

    sets = {"development": pilot, "pgo": [], "held-out": []}
    ranks = Counter()
    for case in mass:
        stage = case["stage"]
        ranks[stage] += 1
        if ranks[stage] <= 8:
            sets["development"].append(case)
        elif ranks[stage] <= 8 + 16:
            sets["pgo"].append(case)
        elif ranks[stage] > sizes[stage] - 32:
            sets["held-out"].append(case)

    cases = [case for group in sets.values() for case in group]
    for key in ("game", "source"):
        if len({case[key] for case in cases}) != len(cases):
            raise ValueError(f"duplicate {key} in reserved games")
    if len({case["dictionary_sha256"] for case in cases}) != 1:
        raise ValueError("reserved games use different dictionaries")
    return sets


def membership(sets):
    return [dict(set=split, game=c["game"], stage=c["stage"], case=case_name(c))
            for split, cases in sets.items() for c in cases]


def reserved_cases(split):
    sets = game_sets()
    with GAME_LIST.open(newline="") as stream:
        if list(csv.DictReader(stream)) != membership(sets):
            raise ValueError(f"corpus membership differs from {GAME_LIST}")
    return sets[split]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--export", choices=SETS, help="write boards and cases.csv for this set")
    parser.add_argument("directory", nargs="?", type=Path)
    args = parser.parse_args()
    if (args.export is None) != (args.directory is None):
        parser.error("supply --export SET DIRECTORY together")
    if args.export:
        write_benchmark(args.directory, reserved_cases(args.export), paired=False)
    else:
        rows = membership(game_sets())
        writer = csv.DictWriter(sys.stdout, fieldnames=rows[0], lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)


if __name__ == "__main__":
    main()
