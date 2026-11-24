"""Select a configurable, stage-balanced sample of recorded game turns."""
import argparse
import csv
import gzip
import json
import random
from itertools import islice
from pathlib import Path
from typing import TypedDict

from replay_game import eligible_turn, load_dictionary, replay, write_board_csv

BENCH = Path(__file__).resolve().parent
PILOT = BENCH / "fixtures/pilot"
STAGES = ("early", "middle", "late")


class Corpus(TypedDict):
    seed: int
    case_count: int
    stage_counts: dict[str, int]
    cases: list[dict]


def solver_board(case):
    return "".join(case["board"]).upper()


def select_turn(turns, stages, seen, rng):
    third, remainder = divmod(len(turns), len(STAGES))
    for stage in stages:
        index = STAGES.index(stage)
        start = index * third + min(index, remainder)
        length = third + (index < remainder)
        first = rng.randrange(length)
        for offset in range(length):
            rank = start + (first + offset) % length
            turn = turns[rank]
            # Both validators see the assigned letter of a played blank.
            key = (solver_board(turn), "".join(sorted(turn["rack"])))
            if key not in seen:
                seen.add(key)
                return stage, rank
    return None


def game_records(games):
    records = []
    paths = set()
    sources = set()
    with games.open(newline="") as stream:
        for record in csv.DictReader(stream):
            path = (games.parent / record["game"]).resolve()
            source = record["source"]
            if path in paths or source in sources:
                raise ValueError(f"duplicate game: {path}")
            paths.add(path)
            sources.add(source)
            # Only the number of available stages matters for ordering. Keep
            # boards for one replay at a time, even for a many-thousand-game pool.
            with path.open(newline="") as game:
                eligible = (number for number, event in enumerate(csv.DictReader(game), 1)
                            if eligible_turn(number, event))
                available = len(list(islice(eligible, len(STAGES))))
            if not available:
                raise ValueError(f"{path}: no eligible post-opening turns")
            records.append((record, path, available))
    if not records:
        raise ValueError("empty games manifest")
    return records


def sample(games, dictionary, seed, size=None) -> Corpus:
    words, fingerprint = load_dictionary(dictionary)
    records = game_records(games)
    if size is None:
        size = len(records)
    if not 1 <= size <= len(records):
        raise ValueError(f"requested size {size}, but need 1 to {len(records)} games")

    rng = random.Random(seed)
    rng.shuffle(records)
    # Give games with fewer than three eligible turns their available stages first.
    records.sort(key=lambda item: item[2])
    counts: dict[str, int] = dict.fromkeys(STAGES, 0)
    third, remainder = divmod(size, len(STAGES))
    quotas = {stage: third + (index < remainder) for index, stage in enumerate(STAGES)}
    cases = []
    seen = set()
    for record, path, available in records:
        stages = [stage for stage in STAGES[:available] if counts[stage] < quotas[stage]]
        if not stages:
            continue
        try:
            turns = list(replay(words, path).values())
        except ValueError as error:
            raise ValueError(f"{path}: {error}") from error
        selection = select_turn(turns, sorted(stages, key=counts.__getitem__), seen, rng)
        if selection is None:
            continue
        stage, rank = selection
        cases.append(dict(**record, stage=stage, eligible_rank=rank + 1, eligible_count=len(turns),
                          dictionary_sha256=fingerprint, **turns[rank]))
        counts[stage] += 1
        if len(cases) == size:
            break
    if len(cases) < size:
        raise ValueError(f"found {len(cases)} of {size} distinct pairs for the stage quotas: {counts}")
    return Corpus(seed=seed, case_count=len(cases), stage_counts=counts, cases=cases)


def write_benchmark(directory, cases):
    rows = [dict(case=f"{c['game']}#{c['turn']}", stage=c["stage"],
                 board=f"board-{i}.csv", rack=c["rack"]) for i, c in enumerate(cases, 1)]
    boards = [solver_board(case) for case in cases]
    for stage in STAGES:
        group = [i for i, row in enumerate(rows) if row["stage"] == stage]
        if len(group) < 2:
            raise ValueError(f"paired benchmark needs two distinct boards in stage {stage}")
        for position, index in enumerate(group):
            for offset in range(1, len(group)):
                companion = group[(position + offset) % len(group)]
                other = rows[companion]
                if boards[companion] != boards[index] and len(other["rack"]) >= 2:
                    rows[index].update(new_board=other["board"], new_board_rack=other["rack"],
                                       additional_rack=other["rack"][:-1])
                    break
            else:
                raise ValueError(f"{rows[index]['case']}: no distinct {stage} board with a reusable rack")
    directory.mkdir(parents=True, exist_ok=True)
    for row, case in zip(rows, cases, strict=True):
        write_board_csv(directory / row["board"], case["board"])
    with (directory / "cases.csv").open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=rows[0], lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)


def verify(games, dictionary, path):
    opener = gzip.open if path.suffix == ".gz" else open
    with opener(path, "rt") as stream:
        saved = json.load(stream)
    corpus = sample(games, dictionary, saved["seed"], saved["case_count"])
    for number, (expected, actual) in enumerate(zip(saved["cases"], corpus["cases"]), 1):
        if expected != actual:
            raise ValueError(f"saved case {number} differs from replay of {actual['game']} turn {actual['turn']}")
    if corpus != saved:
        raise ValueError("saved corpus count or metadata differs from replay")
    return corpus


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--games", type=Path, default=PILOT / "games.csv")
    parser.add_argument("--dictionary", type=Path,
                        default=BENCH.parent / "data/dictionaries/sample_dictionary.txt")
    parser.add_argument("--seed", type=int, help="selection seed (default: 10)")
    parser.add_argument("--size", type=int, help="number of distinct games to sample (default: all)")
    parser.add_argument("--verify", type=Path, help="reproduce and check an ordered saved corpus (.json or .json.gz)")
    parser.add_argument("--benchmark-dir", type=Path, help="also export boards and a paired-benchmark manifest")
    args = parser.parse_args()
    try:
        if args.verify:
            if args.seed is not None or args.size is not None:
                parser.error("--verify reads the seed and size from the saved corpus")
            corpus = verify(args.games, args.dictionary, args.verify)
        else:
            seed = 10 if args.seed is None else args.seed
            corpus = sample(args.games, args.dictionary, seed, args.size)
        if args.benchmark_dir:
            write_benchmark(args.benchmark_dir, corpus["cases"])
    except (ValueError, OSError, KeyError) as error:
        parser.error(str(error))
    if args.verify:
        counts = ", ".join(f"{stage} {count}" for stage, count in corpus["stage_counts"].items())
        print(f"Verified {corpus['case_count']} cases in order ({counts})")
    else:
        print(json.dumps(corpus, indent=2))


if __name__ == "__main__":
    main()
