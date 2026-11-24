# Paired validator benchmark

Run from the repository root with Python 3.10+ and the C++17 toolchain.

## Replay a recorded game

The saved game contains all 20 placements and pre-move racks from Alec Sjoholm vs. Cesar Del Solar. Turns are numbered from one in playing order. The solver can use turns **2–6, 8, 10–16 and 18–20**. Turn 1 opens the board. On turns 7, 9 and 17 the rack contains a blank. Replay still applies those moves so later boards contain the assigned letters. [Recorded racks and moves](fixtures/replay/sjoholm-del-solar.csv) · [Original Macondo game record](https://github.com/domino14/macondo/blob/9fe4fe8cdf3b4e6cbcf42be5f205d463510c2e72/puzzles/testdata/well_played_game.gcg)

```sh
python3 bench/replay_game.py
python3 bench/replay_game.py --turn 18 --board-csv /tmp/turn-18.csv
python3 bench/replay_game.py --turn 18 | cmp - bench/fixtures/replay/turn-18.json
```

The first command lists eligible turns. Selecting a turn prints the player's rack and all 15 board rows immediately before the play. Lowercase letters mark assigned blanks in the JSON. The solver CSV uses uppercase for every occupied square. At turn 18 Cesar held `AFFIIPW`. He placed `WAFFI` down C10–C14 through the E at C15 to form `WAFFIE`. Those five squares are empty in the saved position and occupied before turn 19. [Saved turn 18](fixtures/replay/turn-18.json)

Replay checks rack multiplicities, board connectivity and all complete words after every move. The default fixture dictionary contains the 38 words that occurred in this game. The selected JSON includes the SHA-256 of the dictionary used. With the fixture dictionary, `make test` verifies that both solver validators accept each eligible recorded play at its square with its actual rack. [Fixture dictionary](fixtures/replay/words.txt)

The complete game also passed replay with `--dictionary data/dictionaries/sample_dictionary.txt` (file SHA-256 `559faa1e0296166f8f4c6074b71211c045202accb6920424aaa0758cc7bce9cd`). With that dictionary the packed solver counted 93 across and 110 down placements for turn 18.

## Reproduce the balanced timing pilot

The pilot contains **six recorded games, with two selected turns per stage**. Five games come from Woogles and one is the Sjoholm–Del Solar record above. The local CSVs preserve every placement and both players' pre-move racks. The manifest links each game to its original record. [Game sources](fixtures/pilot/games.csv) · [Saved boards and racks](fixtures/pilot/corpus.json)

| Game | Eligible turns | Selected rank | Recorded turn | Stage | Rack |
| --- | ---: | ---: | ---: | --- | --- |
| 7bbdiRsCRY | 18 | 2 | 4 | Early | EIJKORX |
| RHaUnC67uz | 21 | 11 | 15 | Middle | DEFIIOS |
| 6zTBhes669 | 13 | 13 | 25 | Late | EJLO |
| CpZHNqsaAe | 20 | 7 | 8 | Early | EFGNQUX |
| Sjoholm–Del Solar | 16 | 9 | 12 | Middle | IJNOORU |
| NSFqNG8yP5 | 22 | 21 | 23 | Late | BHILNSU |

The sampler ranks eligible turns from one in playing order and splits them into consecutive thirds. It gives any remainder to the earliest thirds. For example, the 13-turn eligible list splits 5/4/4 and the 20-turn list splits 7/7/6. Seed 10 shuffles the games before stage assignment and selects a turn uniformly within the assigned third. Games with only one or two eligible turns get their available stages first. Each game goes to its least represented available stage, breaking ties in early, middle, late order.

```sh
python3 bench/sample_turns.py --seed 10 | cmp - bench/fixtures/pilot/corpus.json
python3 bench/replay_game.py --game bench/fixtures/pilot/games/6zTBhes669.csv \
    --dictionary data/dictionaries/sample_dictionary.txt --turn 25
python3 bench/sample_turns.py --benchmark-dir /tmp/scrabble-pilot > /tmp/scrabble-pilot.json
```

The saved cases use the full sample dictionary and include its SHA-256. Each `game` path is relative to the input games manifest. To use a different collection or dictionary, pass `--games` or `--dictionary`. The checked-in 252-word dictionary covers every word played or formed in these six games and allows offline replay tests from a clean checkout. [Replay dictionary](fixtures/pilot/words.txt)

The optional benchmark export writes solver CSV boards and `cases.csv` for the paired runner below. Within each stage it uses the next selected board and its actual rack for the new-board workload, wrapping at the end. For the new-rack workload it removes the last recorded tile from that board's rack. This keeps every remaining tile available on that board. This export needs two cases per stage and at least two tiles in each reused rack. Use the same dictionary for sampling and timing.

## Run the paired benchmark

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

## Fixture run

Revision `691c1b0` ran 20 pairs per cache condition in manifest order. Both workers used the same executable, built with Ubuntu Clang 18.1.3 and `-std=c++17 -O2 -Wall -Wextra -Iinclude`, the default glibc 2.39 allocator, and logical CPU 2 of an Intel Core i7-6700K under x86-64 Linux on WSL2. Inputs and temporary caches were on ext4 with the pre-read policy described above.

```sh
taskset -c 2 python3 bench/paired_benchmark.py --repetitions 20 > bench/results/paired-fixture.csv
```

Each entry is an arithmetic mean over the 20 pairs. The last column gives the sample standard deviation of the within-pair differences.

| Pattern cache | Workload | Original (ms) | Packed (ms) | Paired saving (ms) | SD of paired saving (ms) |
| --- | --- | ---: | ---: | ---: | ---: |
| Hit | First board | 0.914035 | 0.887890 | +0.026145 | 0.067659 |
| Hit | New board | 0.016498 | 0.017202 | -0.000704 | 0.001325 |
| Hit | New rack | 0.006164 | 0.005552 | +0.000612 | 0.000128 |
| Miss | First board | 1.135464 | 1.138085 | -0.002621 | 0.167458 |
| Miss | New board | 0.016743 | 0.017141 | -0.000398 | 0.001582 |
| Miss | New rack | 0.006123 | 0.005603 | +0.000520 | 0.000620 |

Both validators agreed on all square masks and all 76 directional candidate decisions across the three measured board/rack combinations. ART on the first board accepted 6 of 28 across and 6 of 26 down. RT on the new board accepted 2 of 4 across and 1 of 5 down. RST on that board accepted 3 of 6 across and 1 of 7 down.

The shared pattern-table phases took 90–93% of mean first-board time across the four validator/cache combinations. First-board paired differences had both signs under each cache condition. These observations describe the seven-word fixture. [Per-case measurements and phase times](results/paired-fixture.csv)
