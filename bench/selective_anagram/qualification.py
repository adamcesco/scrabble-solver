"""Compare the three production solvers with the archived independent decision traces."""

import argparse
import csv
import hashlib
import json
import shutil
import subprocess
import tarfile
import tempfile
from collections import Counter
from pathlib import Path

from .reference import CACHE, COHORTS, DICTIONARY, REVISION, ROOT, check_decisions


SOLVERS = ("rack-neighbor", "packed", "place-and-restore")
REFERENCE_ARCHIVE = ROOT / "bench/results/selective-anagram/reference.tar.gz"


def read_decisions(path):
    decisions = {direction: Counter() for direction in ("across", "down")}
    with path.open() as stream:
        for row in csv.DictReader(stream):
            if row["direction"] not in decisions or row["accepted"] not in ("0", "1"):
                raise ValueError(f"{path}: invalid decision {row}")
            decision = (int(row["row"]), int(row["column"]), row["word"], row["accepted"] == "1")
            decisions[row["direction"]][decision] += 1
    return decisions


def compare_direction(label, reference, production, archived_accepted, returned_accepted):
    if reference != production:
        differences = []
        for kind, delta in (("missing", reference - production), ("extra", production - reference)):
            if delta:
                (row, column, word, accepted), copies = next(iter(delta.items()))
                differences.append(f"{kind} {copies} at ({row},{column}) {word} "
                                   f"{'accepted' if accepted else 'rejected'}")
        raise ValueError(f"{label}: {', '.join(differences)}")

    accepted = sum(copies for (_, _, _, is_accepted), copies in production.items() if is_accepted)
    if archived_accepted != accepted or returned_accepted != accepted:
        raise ValueError(f"{label}: returned {returned_accepted}, trace accepted {accepted}, "
                         f"archive accepted {archived_accepted}")
    return accepted


def build_worker(source, production):
    source.mkdir()
    archive = source.parent / "production.tar"
    subprocess.run(["git", "archive", "--output", archive, production,
                    "Makefile", "src", "include", "tests", "bench/window_counters", "bench/measurement_support"],
                   cwd=ROOT, check=True)
    with tarfile.open(archive) as package:
        package.extractall(source, filter="data")
    # Use the current qualifier with the selected revision's production sources and make rule.
    for name in ("placement_comparison.cpp", "placement_comparison.h"):
        shutil.copyfile(ROOT / "tests" / name, source / "tests" / name)
    shutil.copyfile(Path(__file__).with_suffix(".cpp"), source / "tests/qualification.cpp")
    target = "build/decision_qualification"
    subprocess.run(["make", "-s", target, f"PLACEMENT_EQUIVALENCE_TEST_TARGET={target}",
                    "PLACEMENT_EQUIVALENCE_TEST_SRC=tests/qualification.cpp $(COMPARISON_SRC)"],
                   cwd=source, check=True)
    return source / target


def verify_reference(reference, data):
    provenance = json.loads((reference / "provenance.json").read_text())
    if (provenance["revision"], provenance["solver"]) != (REVISION, "place-and-restore"):
        raise ValueError("expected the pinned place-and-restore reference")
    if hashlib.sha256((data / DICTIONARY).read_bytes()).hexdigest() != provenance["dictionary_sha256"]:
        raise ValueError("dictionary differs from the reference")

    with (reference / "cases.csv").open() as stream:
        cases = list(csv.DictReader(stream))
    if Counter(c["cohort"] for c in cases) != {cohort: limits[0] for cohort, limits in COHORTS.items()}:
        raise ValueError("wrong number of games in the archived cohorts")
    for field in ("board", "case"):
        if len({(c["cohort"], c[field]) for c in cases}) != len(cases):
            raise ValueError(f"duplicate {field} in a cohort")
    for case in cases:
        digest = hashlib.sha256((reference / "inputs" / case["board"]).read_bytes()).hexdigest()
        if digest != case["board_sha256"]:
            raise ValueError(f"{case['case']}: board differs from reference")
        for field in ("across", "down", "across_candidates", "down_candidates"):
            case[field] = int(case[field])
    check_decisions(reference, cases)
    return cases, provenance


def compare_cases(cases, reference, actual):
    with (actual / "counts.csv").open() as stream:
        rows = list(csv.DictReader(stream))
    counts = {(row["board"], row["solver"]): row for row in rows}
    expected = {(case["board"], solver) for case in cases for solver in SOLVERS}
    if len(rows) != len(expected) or counts.keys() != expected:
        raise ValueError(f"incorrect solver count records: missing {expected - counts.keys()}, "
                         f"extra {counts.keys() - expected}, repeated {len(rows) - len(counts)}")

    for case in cases:
        board = case["board"]
        pinned = read_decisions(reference / "decisions" / board)
        for solver in SOLVERS:
            captured = read_decisions(actual / solver / board)
            for direction in ("across", "down"):
                returned = int(counts[board, solver][direction])
                accepted = compare_direction(f"{case['cohort']} {case['case']} {solver} {direction}",
                                             pinned[direction], captured[direction], case[direction], returned)
                yield dict(cohort=case["cohort"], case=case["case"], stage=case["stage"], board=board,
                           solver=solver, direction=direction, reference_decisions=pinned[direction].total(),
                           production_decisions=captured[direction].total(), reference_accepted=case[direction],
                           production_accepted=accepted, returned_accepted=returned, multiset="match",
                           normal_board_restored=True, perpendicular_board_restored=True)


def qualify(data, output, revision):
    if output.exists():
        raise ValueError(f"output directory already exists: {output}")
    output.parent.mkdir(parents=True, exist_ok=True)
    production = subprocess.check_output(["git", "rev-parse", "--verify", revision + "^{commit}"],
                                         cwd=ROOT, text=True).strip()
    with tempfile.TemporaryDirectory(dir=output.parent) as temporary:
        temp = Path(temporary)
        with tarfile.open(REFERENCE_ARCHIVE) as package:
            package.extractall(temp, filter="data")
        reference = temp / "reference"
        cases, provenance = verify_reference(reference, data)
        worker = build_worker(temp / "source", production)
        actual = temp / "actual"
        for solver in SOLVERS:
            for cohort in COHORTS:
                (actual / solver / cohort).mkdir(parents=True)
        requests = "".join(f"{c['case']} {c['board']} {c['rack']}\n" for c in cases)
        with (actual / "counts.csv").open("w") as counts:
            subprocess.run([worker, data / DICTIONARY, data / CACHE, reference / "inputs", actual],
                           input=requests, text=True, stdout=counts, check=True)

        results = list(compare_cases(cases, reference, actual))
        report = temp / "qualification"
        report.mkdir()
        with (report / "cases.csv").open("w", newline="") as stream:
            writer = csv.DictWriter(stream, fieldnames=results[0], lineterminator="\n")
            writer.writeheader()
            writer.writerows(results)
        details = dict(production_revision=production, reference_revision=provenance["revision"],
                       reference_solver=provenance["solver"], dictionary_sha256=provenance["dictionary_sha256"],
                       reference_archive_sha256=hashlib.sha256(REFERENCE_ARCHIVE.read_bytes()).hexdigest(),
                       solvers=SOLVERS, games=len(cases), comparisons=len(results))
        (report / "provenance.json").write_text(json.dumps(details, indent=2) + "\n")
        report.rename(output)
        print(f"Matched {len(results)} directional multisets, counts and board checks in {output}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("data", type=Path, help="full dictionary and v3 cache data directory")
    parser.add_argument("output", type=Path, help="new directory for per-case results")
    parser.add_argument("--production", default="HEAD", help="git revision of the production solvers")
    args = parser.parse_args()
    qualify(args.data.resolve(), args.output.resolve(), args.production)
