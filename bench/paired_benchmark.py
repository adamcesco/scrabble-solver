"""Qualify a manifest, then emit one CSV row per paired workload (times in ms)."""
import argparse
import csv
import hashlib
import io
import itertools
import os
import subprocess
import sys
import tempfile
from collections import Counter
from dataclasses import dataclass
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build"
FIXTURE = ROOT / "bench/fixtures/paired"
FULL_CACHE = ROOT / "data/caches/pattern_and_rack_anagram_to_words.cache"


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def matching_full_cache(worker, dictionary):
    if FULL_CACHE.is_file():
        probe = subprocess.check_output([worker, "--check-cache", dictionary, FULL_CACHE], text=True)
        if probe.strip() == "match":
            return FULL_CACHE
    return None


@dataclass(frozen=True)
class Position:
    board: Path
    rack: str


@dataclass(frozen=True)
class Case:
    name: str
    stage: str
    first: Position
    new_board: Position
    new_rack: Position

    def workloads(self):
        return {"first_board": self.first, "new_board": self.new_board, "new_rack": self.new_rack}


def read_cases(manifest):
    cases = []
    with manifest.open(newline="") as stream:
        for row in csv.DictReader(stream):
            def position(board, rack):
                tiles = row[rack]
                if not 1 <= len(tiles) <= 7 or any(c < "A" or c > "Z" for c in tiles):
                    raise ValueError(f"invalid {rack} in {row['case']}: {tiles!r}")
                path = (manifest.parent / row[board]).resolve(strict=True)
                if any(c in str(path) for c in "\n\r\t"):
                    raise ValueError(f"unsupported control character in board path: {path!r}")
                return Position(path, "".join(sorted(tiles)))

            case = Case(row["case"], row["stage"], position("board", "rack"),
                        position("new_board", "new_board_rack"), position("new_board", "additional_rack"))
            if case.new_rack == case.new_board:
                raise ValueError(f"{case.name}: additional rack must change the tiles")
            cases.append(case)
    if not cases or len({case.name for case in cases}) != len(cases):
        raise ValueError("expected a nonempty manifest with distinct case names")
    return cases


def qualify(dictionary, shared_cache, cases):
    positions = (pos for case in cases for pos in case.workloads().values())
    return qualify_positions(dictionary, shared_cache, positions)


def qualify_positions(dictionary, shared_cache, positions, variants=()):
    subprocess.run([BUILD / "perpendicular_checks_test"], check=True)
    positions = dict.fromkeys(positions)
    requests = "".join(f"{pos.board}\t{pos.rack}\n" for pos in positions)
    output = subprocess.check_output(
        [BUILD / "placement_equivalence_test", dictionary, shared_cache, *variants], input=requests, text=True)
    rows = csv.DictReader(io.StringIO(output))
    return {pos: {key: int(value) for key, value in row.items()}
            for pos, row in zip(positions, rows, strict=True)}


def warm_files(paths):
    for path in dict.fromkeys(paths):
        with path.open("rb") as stream:
            while stream.read(1024 * 1024):
                pass


def observe(variant, condition, dictionary, shared_cache, case, counts):
    # A miss gets a fresh directory even if a previous worker failed after saving.
    with tempfile.TemporaryDirectory() as private:
        cache = shared_cache if condition == "hit" else Path(private) / "patterns"
        inputs = [dictionary, case.first.board, case.new_board.board]
        warm_files([*inputs, cache] if condition == "hit" else inputs)
        output = subprocess.check_output(
            [BUILD / "paired_worker", variant, condition, dictionary, cache, case.first.board, case.first.rack,
             case.new_board.board, case.new_board.rack, case.new_rack.rack], text=True)
        if cache.stat().st_size != shared_cache.stat().st_size:
            raise ValueError(f"{variant}: incomplete pattern cache after {condition}")

    workloads = case.workloads()
    rows = list(csv.DictReader(io.StringIO(output)))
    if [row["workload"] for row in rows] != list(workloads):
        raise ValueError(f"{variant}: missing or out-of-order observations")
    timings = {}
    for row in rows:
        workload = row.pop("workload")
        expected = counts[workloads[workload]]
        if row.pop("cache") != condition:
            raise ValueError(f"{variant}: expected pattern-cache {condition}")
        if any(int(row.pop(key)) != expected[key] for key in ("across", "down")):
            raise ValueError(f"{case.name} {variant} {workload}: timed counts differ from preflight")
        timings[workload] = {f"{variant}_{key}": value for key, value in row.items()}
    return timings


def paired_rows(dictionary, cache, cases, repetitions, counts, conditions):
    for condition in conditions:
        group_indices = Counter()
        for repetition, case in itertools.product(range(1, repetitions + 1), cases):
            variants = ("original", "packed")
            if group_indices[case.stage] % 2:
                variants = variants[::-1]
            group_indices[case.stage] += 1
            timings = {v: observe(v, condition, dictionary, cache, case, counts) for v in variants}
            for workload, pos in case.workloads().items():
                original, packed = (timings[v][workload] for v in ("original", "packed"))
                saving = float(original["original_total_ms"]) - float(packed["packed_total_ms"])
                yield dict(case=case.name, stage=case.stage, repetition=repetition, cache=condition,
                           workload=workload, board=os.path.relpath(pos.board), rack=pos.rack,
                           first_validator=variants[0],
                           **counts[pos], **original, **packed, saving_ms=f"{saving:.6f}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dictionary", type=Path, default=FIXTURE / "dictionary.txt")
    parser.add_argument("--cases", type=Path, default=FIXTURE / "cases.csv")
    parser.add_argument("--repetitions", type=int, default=4)
    parser.add_argument("--cache", type=Path, help="existing pattern cache for qualification and hits")
    parser.add_argument("--condition", choices=("hit", "miss", "both"),
                        help="defaults to hit with an existing cache, both with a temporary cache")
    args = parser.parse_args()
    if args.repetitions < 1:
        parser.error("--repetitions must be positive")
    cases = read_cases(args.cases)
    with tempfile.TemporaryDirectory() as directory:
        if args.cache is not None and not args.cache.is_file():
            parser.error(f"pattern cache missing: {args.cache}")
        existing = args.cache or matching_full_cache(BUILD / "paired_worker", args.dictionary)
        cache = existing or Path(directory) / "shared-patterns"
        counts = qualify(args.dictionary, cache, cases)
        writer = None
        condition = args.condition or ("hit" if existing else "both")
        conditions = ("hit", "miss") if condition == "both" else (condition,)
        for row in paired_rows(args.dictionary, cache, cases, args.repetitions, counts, conditions):
            if writer is None:
                writer = csv.DictWriter(sys.stdout, fieldnames=row, lineterminator="\n")
                writer.writeheader()
            writer.writerow(row)
            sys.stdout.flush()


if __name__ == "__main__":
    main()
