# Paired validator benchmark

Run from the repository root with Python 3.10+ and the C++17 toolchain.

```sh
make -s paired-benchmark > pairs.csv
```

The default fixture opens with AS across the center. Playing AT below the S forms SAT, then playing O beside the T forms TO. The first board uses rack ART. The second board uses RT, then RST. The seven-word dictionary includes RAT and TAR so the second board exercises both accepted and rejected crossings.

To measure a corpus, supply a CSV manifest and a dictionary. Each CSV row describes one pair of worker processes. Board paths are relative to the manifest. Use one row per case and the same stage for its first and subsequent board. The `additional_rack` is played on `new_board`. Rack strings contain one to seven uppercase letters in any order, including repeats.

```csv
case,stage,board,rack,new_board,new_board_rack,additional_rack
as-to-sat,fixture,opening.csv,ART,crossings.csv,RT,RST
```

```sh
make -s paired-benchmark BENCH_ARGS='--cases corpus/cases.csv --dictionary corpus/words.txt --repetitions 1' > pairs.csv
```

Before timing, the runner executes the crossing and placement fixtures. It qualifies every distinct measured board and rack against the supplied dictionary. In each view it checks every empty square against 26 independent tile trials and compares sorted candidate decisions including coordinates, word, outcome and duplicates. The qualification process also builds the shared pattern cache.

Each fresh worker runs one validator through all three workloads. The first-board clock starts before dictionary reading. The new-board clock starts before loading the second CSV with the dictionary tables retained. The new-rack clock starts before building its subsets on the prepared second board. All clocks stop after the across count, down count and view restoration. The worker prints after the third solve. The runner checks both timed counts against the qualification results before emitting a pair.

Both validators run sequentially from the same binary. The runner keeps manifest order and alternates the first validator within each stage and cache condition. It reads the dictionary and both board files immediately before each worker. For a cache hit it also reads the same shared cache file. For a miss it supplies a fresh empty temporary directory per worker. It checks the actual load/build branch and the saved file size. Temporary caches use the system temporary directory (overridable with `TMPDIR`).

The CSV contains one row per case, repetition, cache condition and workload. Counts are per direction. Each validator's columns give direct elapsed milliseconds and the component times. The load column includes the failed open on a miss. Build and save are separate. Solve includes rack subsets and both passes through restoration. A zero component means the workload skipped that phase. The cache label on the reuse rows identifies how the worker obtained its dictionary tables. Paired saving is original elapsed time minus packed elapsed time.
