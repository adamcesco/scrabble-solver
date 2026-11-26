"""Frozen candidate-decision reference for single-solver qualification."""
import csv
from collections import Counter

from bench.measurement_support.benchmark_inputs import ROOT

REVISION = "823763cf735c1930fd357cff0bd5e155aa47341d"
DICTIONARY = "dictionaries/sample_dictionary.txt"
CACHE = "caches/pattern_and_rack_anagram_to_words.cache"
# Games, candidate decisions, and accepted decisions in the four-way archive.
COHORTS = {"development": (30, 72530, 12939), "held-out": (96, 229655, 41494)}


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
