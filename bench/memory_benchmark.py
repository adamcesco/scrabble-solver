"""Measure peak and retained bytes in separate original and packed worker processes."""
import argparse
import csv
import hashlib
import os
import subprocess
import sys
import tempfile
from pathlib import Path

from paired_benchmark import BUILD, ROOT, matching_full_cache, warm_files

FULL_DICTIONARY = ROOT / "data/dictionaries/sample_dictionary.txt"
SAMPLE_BOARD = ROOT / "data/boards/sample_board.csv"


def observe(worker):
    blocks = {}
    live = peak = shared_heap = dictionary_heap = 0
    sizes = {}
    counts = None
    for line in worker.stdout:
        event, *args = line.split()
        if event == "+":
            address, size = int(args[0], 16), int(args[1])
            if address in blocks:
                raise ValueError("allocation at a live address")
            blocks[address] = size
            live += size
            peak = max(peak, live)
        elif event == "-":
            # WordTable frees getline's libc-owned input buffers. Only allocations
            # requested by the solver contribute to its retained or lookup bytes.
            address = int(args[0], 16)
            if address not in blocks and "retained_shared_dictionary" in sizes:
                raise ValueError("untraced release during validator or board construction")
            live -= blocks.pop(address, 0)
        elif event == "scratch":
            peak = max(peak, live + int(args[0]))
        elif event == "shared":
            shared_heap = peak = live
            sizes["retained_shared_dictionary"] = live + int(args[0])
        elif event == "validator":
            inline, rss = map(int, args)
            dictionary_heap = live
            sizes["validator_construction_peak"] = peak - shared_heap + inline
            sizes["retained_validator"] = live - shared_heap + inline
            sizes["retained_dictionary"] = sizes["retained_shared_dictionary"] + sizes["retained_validator"]
            sizes["dictionary_peak_rss"] = rss
        elif event == "board":
            sizes["retained_board"] = live - dictionary_heap + int(args[0])
        elif event == "solved":
            cache, across, down, rss = args
            counts = (cache, int(across), int(down))
            sizes["first_solve_peak_rss"] = int(rss)
        else:
            raise ValueError(f"unknown allocation trace record: {line!r}")
    if worker.wait():
        raise subprocess.CalledProcessError(worker.returncode, worker.args)
    if counts is None:
        raise ValueError("worker did not finish the first solve")
    return counts, sizes


def measure(variant, dictionary, board, rack, shared_cache):
    with tempfile.TemporaryDirectory() as directory:
        hit = shared_cache is not None
        cache = shared_cache if hit else Path(directory) / "patterns"
        warm_files([dictionary, board, cache] if hit else [dictionary, board])
        command = [BUILD / "memory_worker", variant, "hit" if hit else "miss",
                   dictionary, cache, board, rack]
        with subprocess.Popen(command, stdout=subprocess.PIPE, text=True) as worker:
            return observe(worker)


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dictionary", type=Path, default=FULL_DICTIONARY)
    parser.add_argument("--board", type=Path, default=SAMPLE_BOARD)
    parser.add_argument("--rack", default="ABFHIKU")
    parser.add_argument("--cache", type=Path, help="existing pattern cache for both workers")
    parser.add_argument("--repetitions", type=int, default=1, help="matched pairs, alternating first validator")
    args = parser.parse_args()
    if args.repetitions < 1:
        parser.error("--repetitions must be positive")
    cache = args.cache
    dictionary_sha = sha256(args.dictionary)
    if cache is None:
        cache = matching_full_cache(BUILD / "memory_worker", args.dictionary)
    if cache is not None and not cache.is_file():
        parser.error(f"pattern cache missing: {cache}")
    rack = "".join(sorted(args.rack))
    metadata = dict(dictionary=os.path.relpath(args.dictionary), dictionary_sha256=dictionary_sha,
                    board=os.path.relpath(args.board), board_sha256=sha256(args.board), rack=rack,
                    cache_file=os.path.relpath(cache) if cache else "")
    writer = csv.DictWriter(sys.stdout, [*metadata, "cache", "repetition", "first_validator", "across",
                                         "down", "metric", "original_bytes", "packed_bytes",
                                         "packed_minus_original_bytes"], lineterminator="\n")
    order = ("original", "packed")
    for repetition in range(1, args.repetitions + 1):
        pair = {variant: measure(variant, args.dictionary, args.board, rack, cache) for variant in order}
        original_counts, original = pair["original"]
        packed_counts, packed = pair["packed"]
        if original_counts != packed_counts:
            raise ValueError(f"cache or directional counts differ: {original_counts} / {packed_counts}")
        if original["retained_shared_dictionary"] != packed["retained_shared_dictionary"]:
            raise ValueError("workers retained different shared dictionary sizes")
        condition, across, down = original_counts
        if repetition == 1:
            writer.writeheader()
        for metric, before in original.items():
            writer.writerow(dict(metadata, cache=condition, repetition=repetition, first_validator=order[0],
                                 across=across, down=down, metric=metric, original_bytes=before,
                                 packed_bytes=packed[metric], packed_minus_original_bytes=packed[metric] - before))
        order = order[::-1]


if __name__ == "__main__":
    main()
