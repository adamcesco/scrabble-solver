"""Select one recorded turn per game for a stage-balanced timing pilot."""
import argparse
import csv
import json
import random
from pathlib import Path

from replay_game import load_dictionary, replay, write_board_csv

BENCH = Path(__file__).resolve().parent
PILOT = BENCH / "fixtures/pilot"
STAGES = ("early", "middle", "late")


def sample(games, dictionary, seed):
    words, fingerprint = load_dictionary(dictionary)
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
            try:
                turns = list(replay(words, path).values())
            except ValueError as error:
                raise ValueError(f"{path}: {error}") from error
            if not turns:
                raise ValueError(f"{path}: no eligible post-opening turns")
            records.append((record, turns))
    if not records:
        raise ValueError("empty games manifest")

    rng = random.Random(seed)
    rng.shuffle(records)
    # Give games with fewer than three eligible turns their available stages first.
    records.sort(key=lambda item: min(len(item[1]), 3))
    counts = dict.fromkeys(STAGES, 0)
    cases = []
    for record, turns in records:
        available = STAGES[:len(turns)]
        stage = min(available, key=counts.__getitem__)
        index = STAGES.index(stage)
        third, remainder = divmod(len(turns), 3)
        start = index * third + min(index, remainder)
        rank = rng.randrange(start, start + third + (index < remainder))
        cases.append(dict(**record, stage=stage, eligible_rank=rank + 1, eligible_count=len(turns),
                          dictionary_sha256=fingerprint, **turns[rank]))
        counts[stage] += 1
    if max(counts.values()) - min(counts.values()) > 1:
        raise ValueError("too few eligible turns to balance these games across stages")
    return dict(seed=seed, case_count=len(cases), stage_counts=counts, cases=cases)


def write_benchmark(directory, cases):
    rows = [dict(case=f"{c['game']}#{c['turn']}", stage=c["stage"],
                 board=f"board-{i}.csv", rack=c["rack"]) for i, c in enumerate(cases, 1)]
    for stage in STAGES:
        group = [row for row in rows if row["stage"] == stage]
        if len(group) < 2:
            raise ValueError(f"paired benchmark needs two distinct boards in stage {stage}")
        for row, other in zip(group, group[1:] + group[:1]):
            # Exercise rack reuse with a proper subset of the actual rack on this board.
            additional = other["rack"][:-1]
            if not additional:
                raise ValueError(f"{other['case']}: cannot shorten a one-tile rack for reuse")
            row.update(new_board=other["board"], new_board_rack=other["rack"], additional_rack=additional)
    directory.mkdir(parents=True, exist_ok=True)
    for row, case in zip(rows, cases, strict=True):
        write_board_csv(directory / row["board"], case["board"])
    with (directory / "cases.csv").open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=rows[0], lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--games", type=Path, default=PILOT / "games.csv")
    parser.add_argument("--dictionary", type=Path,
                        default=BENCH.parent / "data/dictionaries/sample_dictionary.txt")
    parser.add_argument("--seed", type=int, default=10)
    parser.add_argument("--benchmark-dir", type=Path, help="also export boards and a paired-benchmark manifest")
    args = parser.parse_args()
    try:
        corpus = sample(args.games, args.dictionary, args.seed)
        if args.benchmark_dir:
            write_benchmark(args.benchmark_dir, corpus["cases"])
    except (ValueError, OSError) as error:
        parser.error(str(error))
    print(json.dumps(corpus, indent=2))


if __name__ == "__main__":
    main()
