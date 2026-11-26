"""Capture place-and-restore decisions from the pinned revision on both archived cohorts."""

import argparse
import csv
import json
import shutil
import subprocess
import sys
import tempfile
from collections import Counter
from pathlib import Path

from bench.measurement_support.benchmark_inputs import ROOT, sha256

REVISION = "823763cf735c1930fd357cff0bd5e155aa47341d"
DICTIONARY = "dictionaries/sample_dictionary.txt"
CACHE = "caches/pattern_and_rack_anagram_to_words.cache"
# Games, candidate decisions, and accepted decisions in the four-way archive.
COHORTS = {"development": (30, 72530, 12939), "held-out": (96, 229655, 41494)}


def prepare_cases(source, data, output):
    cases = []
    dictionary_hash = sha256(data / DICTIONARY)
    for cohort, (games, _, _) in COHORTS.items():
        inputs = output / "inputs" / cohort
        subprocess.run([sys.executable, "-m", "bench.window_counters.window_games", "--export", cohort, inputs],
                       cwd=source, check=True)
        previous = source / "bench/results/window-finalists"
        if cohort == "held-out":
            previous /= cohort
        archive = json.loads((previous / "manifest.json").read_text())
        if archive["input_sha256"][f"data/{DICTIONARY}"] != dictionary_hash:
            raise ValueError("dictionary SHA-256 differs from the archive")
        with (inputs / "cases.csv").open() as stream:
            positions = list(csv.DictReader(stream))
        identities = {p["case"] for p in positions}
        if len(positions) != games or identities != archive["inputs"].keys() or identities != archive["counts"].keys():
            raise ValueError(f"{cohort}: replayed cases differ from archive")
        for position in positions:
            name = position["case"]
            rack = "".join(sorted(position["rack"]))
            board = inputs / position["board"]
            digest = sha256(board)
            identity = archive["inputs"][name]
            if (digest, rack, position["stage"]) != (identity["board_sha256"], identity["rack"], identity["stage"]):
                raise ValueError(f"{name}: board, rack or stage differs from archive")
            cases.append(dict(cohort=cohort, case=name, stage=position["stage"],
                              board=str(Path(cohort) / board.name), board_sha256=digest,
                              rack=rack, **archive["counts"][name]))
        (output / "decisions" / cohort).mkdir(parents=True)
    return cases, dictionary_hash


def check_decisions(output, cases):
    totals = {cohort: Counter() for cohort in COHORTS}
    for case in cases:
        counts = Counter()
        with (output / "decisions" / case["board"]).open() as stream:
            for row in csv.DictReader(stream):
                direction = row["direction"]
                if direction not in ("across", "down") or row["accepted"] not in ("0", "1"):
                    raise ValueError(f"{case['case']}: invalid decision {row}")
                counts[f"{direction}_candidates"] += 1
                counts[direction] += int(row["accepted"])
        for field in ("across", "across_candidates", "down", "down_candidates"):
            if counts[field] != case[field]:
                raise ValueError(f"{case['case']}: {counts[field]} {field}, expected {case[field]}")
        totals[case["cohort"]].update(counts)
    for cohort, counts in totals.items():
        decisions = counts["across_candidates"] + counts["down_candidates"]
        accepted = counts["across"] + counts["down"]
        if (decisions, accepted) != COHORTS[cohort][1:]:
            raise ValueError(f"{cohort}: {decisions} decisions, {accepted} accepted differ from archive")
        print(f"{cohort}: {decisions} decisions, {accepted} accepted")


def capture(data, destination):
    if destination.exists():
        raise ValueError(f"output directory already exists: {destination}")
    destination.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=destination.parent) as temporary:
        temp = Path(temporary)
        source = temp / "source"
        source.mkdir()
        # Compile and replay from exactly this commit even in a dirty or newer checkout.
        archive = temp / "source.tar"
        subprocess.run(["git", "archive", "--output", archive, REVISION], cwd=ROOT, check=True)
        subprocess.run(["tar", "-xf", archive, "-C", source], check=True)
        output = temp / "reference"
        cases, dictionary_hash = prepare_cases(source, data, output)
        # Use the pinned qualifier's compile rule, replacing just its command-line driver.
        shutil.copyfile(Path(__file__).with_suffix(".cpp"), source / "tests/reference.cpp")
        worker = Path("build/placement_equivalence_test")
        subprocess.run(["make", "-s", worker,
                        "PLACEMENT_EQUIVALENCE_TEST_SRC=tests/reference.cpp $(COMPARISON_SRC)"],
                       cwd=source, check=True)
        requests = "".join(f"{case['board']} {case['rack']}\n" for case in cases)
        subprocess.run([source / worker, data / DICTIONARY, data / CACHE], input=requests, text=True, cwd=output, check=True)
        check_decisions(output, cases)
        with (output / "cases.csv").open("w", newline="") as stream:
            writer = csv.DictWriter(stream, fieldnames=cases[0], lineterminator="\n")
            writer.writeheader()
            writer.writerows(cases)
        provenance = {"solver": "place-and-restore", "revision": REVISION, "dictionary_sha256": dictionary_hash}
        (output / "provenance.json").write_text(json.dumps(provenance, indent=2) + "\n")
        output.rename(destination)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("data", type=Path, help="data directory containing the dictionary and its existing v3 cache")
    parser.add_argument("output", type=Path, help="new directory for per-case inputs and decisions")
    args = parser.parse_args()
    capture(args.data.resolve(), args.output.resolve())
