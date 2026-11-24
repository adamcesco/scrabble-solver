# Paired validator benchmark

Run from the repository root with Python 3.10+ and the C++17 toolchain.

## Compare window counters on prepared boards

`make -s window-benchmark` qualifies and times `packed` and `one-gap` on the small fixture. Use `--variants BASELINE CHALLENGER` to select a different pair. The runner takes the primary board and rack of each case in the paired manifest. Each CSV row records both solve times in microseconds, their difference (baseline minus challenger), the run order, stage, directional counts, input fingerprints, compiler flags, worker fingerprint and CPU affinity.

The qualifier compares each counter with place-and-restore on every measured position. It checks every empty square's allowed letters and compares candidate coordinates, words and decisions with duplicates retained in each direction. It also checks the restored rows and prepared grid identities, and the packed counter's lookup counts. The timing worker loads the same dictionary and pattern table once and prepares the board before each pair. Each pair runs two warm-up rounds, then one measured round. Both counters solve the same prepared board in the assigned order in all three rounds. The clock covers rack-subset generation, both directional counts and the final view swap. The runner checks both release counts against qualification before writing the CSV. Within each stage and repetition, successive cases alternate which counter goes first. Each case also reverses order on its next repetition.

The recorded runs used logical CPU 2 on an i7-6700K. Select an available CPU on another host. The replay command verifies and exports the six pilot positions using the full project dictionary.

```sh
flags='-std=c++17 -O2 -march=x86-64 -Wall -Wextra -Iinclude'
taskset -c 2 make -s window-benchmark CXX=clang++ CXXFLAGS="$flags" \
    BENCH_ARGS='--variants packed one-gap --repetitions 20' > bench/results/window-fixture.csv
python3 bench/sample_turns.py --verify bench/fixtures/pilot/corpus.json --benchmark-dir build/timing-pilot
taskset -c 2 make -s window-benchmark CXX=clang++ CXXFLAGS="$flags" \
    BENCH_ARGS='--variants packed one-gap --repetitions 20 --cases build/timing-pilot/cases.csv --dictionary data/dictionaries/sample_dictionary.txt' \
    > bench/results/window-pilot.csv
```

To add a counter, define its window callable beside `PackedCounter` and register `measure_solve<YourCounter>` under a name in `window_variant`. The qualifier and worker compile that registry. The production entry calls the packed counter directly. [Counter implementations and registry](../src/rack_oriented_solver.cpp)

The build keeps each compiler and flag configuration in its own directory. Run `make -s window-tools CXX=clang++ CXXFLAGS="$flags"` to print the directory containing the qualifier and worker. In the recorded Clang 18.1.3 worker, `objdump -d -C` shows separate timed drivers at `0x3cf0` and `0x3de0`. Each calls rack-subset generation immediately after the start clock and reads the end clock after the second view swap. Both the production packed entry and the packed driver call the traversal at `0x3f00`. In the one-gap traversal the binary-probe and merge paths test the key letter with `bt` at `0x5002` and `0x523e`, then branch around the addition of the group count on a miss. Neither path fetches a candidate word. Multi-gap matches call the packed candidate check directly. The variant name lookup is outside both timed drivers.

### One-gap results

| Position set | Pairs | Packed mean (µs) | One-gap mean (µs) | Packed minus one-gap (µs) |
| --- | ---: | ---: | ---: | ---: |
| Small fixture | 20 | 8.46 | 8.16 | +0.30 |
| Early pilot | 40 | 403.33 | 418.45 | −15.13 |
| Middle pilot | 40 | 502.41 | 513.67 | −11.26 |
| Late pilot | 40 | 264.05 | 258.36 | +5.69 |
| **Pilot, equal stage weights** | **120** | **389.93** | **396.83** | **−6.90** |

With the full 191,852-word dictionary, one-gap added 6.90 µs per solve (1.8%) on the six pilot positions. It saved time on both late positions. The 95% stratified bootstrap interval for packed minus one-gap was −10.10 to −3.70 µs, averaging repetitions per game before resampling the two games per stage (20,000 draws, seed 15). The small fixture's 0.30 µs mean saving had a within-pair difference standard deviation of 2.75 µs. [Pilot pairs](results/window-pilot.csv) · [Fixture pairs](results/window-fixture.csv)

Order affected these warm solves. Mean packed minus one-gap on the pilot was +1.25 µs with packed first and −15.05 µs with one-gap first. Each game contributed ten pairs in each order. The interval above describes variation across these six game means. Held-out games are needed to rank finalists.

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

The byte comparison below requires the project's separately supplied `data/dictionaries/sample_dictionary.txt` with SHA-256 `559faa1e0296166f8f4c6074b71211c045202accb6920424aaa0758cc7bce9cd`. Each saved case records this fingerprint.

```sh
python3 bench/sample_turns.py --seed 10 | cmp - bench/fixtures/pilot/corpus.json
python3 bench/replay_game.py --game bench/fixtures/pilot/games/6zTBhes669.csv \
    --dictionary data/dictionaries/sample_dictionary.txt --turn 25
python3 bench/sample_turns.py --benchmark-dir /tmp/scrabble-pilot > /tmp/scrabble-pilot.json
```

From a clean checkout, the following command verifies every saved board, rack and stage using the bundled 252-word dictionary. It covers every word played or formed in these six games. Each `game` path is relative to the input games manifest. To use a different collection or dictionary, pass `--games` or `--dictionary`. [Replay dictionary](fixtures/pilot/words.txt)

```sh
python3 tests/sample_turns_test.py SampleTurnsTest.test_replays_every_saved_case_with_the_fixture_dictionary
```

The optional benchmark export writes solver CSV boards and `cases.csv` for the paired runner below. For each case it takes the next different board in the same stage whose rack has at least two tiles, wrapping at the end. For the new-rack workload it removes the last recorded tile from that board's rack. A one-tile rack is still eligible for the first-board workload. Each case needs a suitable companion for export. Use the same dictionary for sampling and timing.

## Replay or resize the mass-scale corpus

The saved corpus contains **10,000 distinct board-and-rack pairs from 10,000 recorded Quackle self-play games**. Seed 11 selected 3,334 early, 3,333 middle and 3,333 late positions. Each case includes its pre-move board, actual rack, turn number, rank among eligible turns, stage, source URL and sample dictionary SHA-256 (`559faa1e0296166f8f4c6074b71211c045202accb6920424aaa0758cc7bce9cd`). [Ordered cases](fixtures/mass/corpus.json.gz) · [Game manifest and recordings](fixtures/mass/games.tar.gz)

With the project dictionary in `data/dictionaries/sample_dictionary.txt`, these commands reproduce the selection and compare every case in order, including the dictionary fingerprint. The optional export gives both validators the same ordered manifest.

```sh
mkdir -p build/mass-games
tar -xzf bench/fixtures/mass/games.tar.gz -C build/mass-games
python3 bench/sample_turns.py --games build/mass-games/games.csv \
    --verify bench/fixtures/mass/corpus.json.gz --benchmark-dir build/mass-paired
```

Set `--size` to the number of distinct games required. For example, the following selects 12,000 positions from the saved pool. The sampler gives leftover cases to the earlier stages. It compares boards as the solver reads them and racks by sorted letters, retaining every copy. If a pair repeats, it tries another eligible turn or game. If the pool cannot fill the quotas, the command exits with an error.

```sh
python3 bench/sample_turns.py --games build/mass-games/games.csv \
    --seed 11 --size 12000 > build/corpus-12000.json
```

The bundled pool contains **12,592 games**. For larger samples, supply a larger `game,source` manifest with one recording per game. Paths are relative to the manifest. To inspect an individual selection, pass its game path and turn to `bench/replay_game.py` with the same dictionary.

The recordings come from Kevin McElwee's tile-value experiment. Of the selected games, 3,645 used traditional scoring and 6,355 used alternate tile values. The importer keeps complete games containing only placements and replays every move against the sample dictionary. This excludes games with exchanges or passes. The benchmark consumes board letters and actual racks. The tile values influence which moves Quackle played. [Pinned original game records](https://github.com/kmcelwee/scrabble/tree/b4c0ccf7c6db50399a3f867cb359b45c861292f0/data)

To rebuild the source pool from the original archive, run the following commands. The importer checks the archive SHA-256 before reading the recordings.

```sh
curl -fL https://codeload.github.com/kmcelwee/scrabble/tar.gz/b4c0ccf7c6db50399a3f867cb359b45c861292f0 \
    -o build/quackle-games.tar.gz
python3 bench/import_quackle_games.py build/quackle-games.tar.gz build/imported-games
diff -qr build/mass-games build/imported-games
```

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

Before timing, the runner executes the crossing and placement fixtures. It qualifies every distinct measured board and rack against the supplied dictionary. In each view it checks every empty square against 26 independent tile trials and compares sorted candidate decisions including coordinates, word, outcome and duplicates. Qualification loads the supplied `--cache`, or automatically uses the project pattern cache when its header matches the normalized dictionary. It checks the payload with the solver's loader. With no matching cache it builds one temporary shared cache. Qualification never overwrites an existing cache.

Each fresh worker runs one validator through all three workloads. The first-board clock starts before dictionary reading. The new-board clock starts before loading the second CSV with the dictionary tables retained. The new-rack clock starts before building its subsets on the prepared second board. All clocks stop after the across count, down count and view restoration. The worker prints after the third solve. The runner checks both timed counts against the qualification results before emitting a pair.

Both validators run sequentially from the same binary. The runner keeps manifest order and alternates the first validator within each stage and cache condition. It reads the dictionary and both board files immediately before each worker. For a cache hit it also reads the same shared cache file. For a miss it supplies a fresh empty temporary directory per worker. It checks the actual load/build branch and the saved file size. Temporary caches use the system temporary directory (overridable with `TMPDIR`). An existing shared cache defaults to hit-only timing. With a temporary shared cache the default exercises both conditions. Use `--condition hit`, `miss` or `both` to choose explicitly. A deliberate timed miss still qualifies against the shared cache.

The CSV contains one row per case, repetition, cache condition and workload. Counts are per direction. Each validator's columns give direct elapsed milliseconds and the component times. The load column includes the failed open on a miss. Build and save are separate. Solve includes rack subsets and both passes through restoration. A zero component means the workload skipped that phase. The cache label on the reuse rows identifies how the worker obtained its dictionary tables. Paired saving is original elapsed time minus packed elapsed time.

## Size and run the verified corpus comparison

Use the full project dictionary for both the six-game pilot and the verified Quackle corpus. Run the following from the repo root. Change the logical CPU to one available on your machine. Each pilot and each full comparison finishes qualification before starting any timed worker.

```sh
make build/placement_equivalence_test build/perpendicular_checks_test build/paired_worker
python3 bench/sample_turns.py --verify bench/fixtures/pilot/corpus.json --benchmark-dir build/timing-pilot
dictionary=data/dictionaries/sample_dictionary.txt
for condition in hit miss; do
    pilot=bench/results/pilot-$condition.csv
    pairs=bench/results/mass-$condition.csv
    cases=build/timing-$condition
    taskset -c 2 python3 bench/paired_benchmark.py --dictionary "$dictionary" \
        --condition "$condition" --repetitions 1 --cases build/timing-pilot/cases.csv > "$pilot" || break
    python3 bench/mass_comparison.py --condition "$condition" --pilot "$pilot" --benchmark-dir "$cases" || break
    taskset -c 2 python3 bench/paired_benchmark.py --dictionary "$dictionary" \
        --condition "$condition" --repetitions 1 --cases "$cases/cases.csv" > "$pairs" || break
    python3 bench/mass_comparison.py --condition "$condition" --pilot "$pilot" --report "$pairs" || break
done
```

For each cache condition, the pilot estimates the variance of **original minus packed** first-board milliseconds within each stage. Let $T$ be 1% of the equally weighted original mean, and $s_h^2$ the sample variance for stage $h$. For $m$ independent games per stage, the estimated variance of the equally weighted saving is $\sum_h s_h^2/(9m)$. Set $m$ to the ceiling of $1.96^2 \sum_h s_h^2/(9T^2)$ with a minimum of two games for the order crossover. This sizes the verified 10,000-game starting corpus independently for hits and misses. Take the first $m$ saved cases of each stage in corpus order and use the same-stage companion and additional rack from the benchmark export. The six pilot games are separate from the full estimates.

The report uses 20,000 bootstrap draws with seed 15. Each draw samples whole paired game differences with replacement within each stage at its observed size, then averages the three stage means equally. For reuse, the measured game is the companion that provided the board. If a one-tile rack caused the exporter to reuse a companion, average its repeat timings before computing means, ranges and intervals so the game receives one vote. The 2.5th and 97.5th percentiles give the interval. The reported spread is the observed range of paired game differences. The final half-width is checked against the full-run original mean. Pilot sizing estimates precision from just two games per stage, so the final interval can miss its target. Break-even uses the ceiling of the observed first-board premium divided by the mean saving per additional board or rack. The report distinguishes intervals spanning zero from reliably positive and reliably negative reuse savings.

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

## Measure peak and retained memory

```sh
make -s memory-benchmark BENCH_ARGS='--repetitions 2' > memory.csv
```

The memory benchmark runs on x86-64 Linux with GNU linker wrapping and libstdc++. Each worker constructs one validator and solves one board in both directions. The default inputs are the full project dictionary, sample board and rack `ABFHIKU`. The runner checks the header of `data/caches/pattern_and_rack_anagram_to_words.cache` against the loaded dictionary’s word count and fingerprint. Both workers use that cache when it matches, including for equivalent dictionary files with different line endings or case. To measure another input, supply `--dictionary`, `--board`, `--rack` and optionally `--cache`. A supplied cache must pass the solver's load checks. On a cache miss each worker builds and saves into its own empty temporary directory.

Before each worker the runner reads the dictionary, board and any shared cache into the filesystem cache. Workers run sequentially, alternating first validator across pairs. The CSV records input paths and SHA-256 hashes for the dictionary and board, the rack, the cache path and verified condition, both directional counts and packed minus original bytes. The runner checks the counts and shared dictionary size before emitting each pair.

| Metric | Boundary and accounting |
| --- | --- |
| Dictionary peak RSS | Linux `/proc/self/status` process high-water mark (`VmHWM` in KiB × 1,024), sampled immediately after constructing the active validator. |
| First-solve peak RSS | The same cumulative high-water mark after rack subsets, both directional solves and view restoration, before output of the counts and teardown. |
| Validator construction peak | Maximum simultaneously live requested bytes during lookup construction plus the active table's inline size. The original builder's 16-byte tile buffer and 48-byte temporary row are live alongside its output table. A packed hash-table rehash charges both bucket arrays until the old one is released. |
| Retained dictionary | Shared word and pattern tables plus the active validator. The CSV also breaks out each portion. Counts include inline objects and full allocation requests for word storage, vectors, hash nodes and bucket arrays. Borrowed word views add no second copy of the letters. |
| Incremental retained board | One board's inline size plus its live allocations after preparation. For packed this includes both run-start objects, their four ID arrays and both shared-pointer control blocks. The lookup belongs to the dictionary. |

The worker forwards the solver's `malloc`, `calloc`, scalar `new` and corresponding releases to the normal allocator. A separate reader tallies the addresses and requested sizes as they arrive. This keeps the allocation ledger outside the measured process and captures spare capacity and overlapping old and new allocations. RSS includes resident stack pages, allocator overhead and the worker's own I/O. Owned-byte totals count data structures and explicit construction buffers at requested capacity. Allocator rounding, call frames and library I/O fall outside those totals.

### Full dictionary, sample board

In two matched pairs, all four workers loaded the same 13,639,098,672-byte pattern cache for the 191,852-word dictionary and counted 254 across and 102 down with rack `ABFHIKU`. The workers used the same executable, built with Ubuntu Clang 18.1.3 at `-O2` on x86-64 Linux on WSL2 with glibc 2.39. [Paired byte measurements and input fingerprints](results/memory-sample.csv)

| Bytes | Original | Packed | Packed − original |
| --- | ---: | ---: | ---: |
| Peak resident through dictionary construction, pair 1 | 18,016,313,344 | 17,892,327,424 | −123,985,920 |
| Peak resident through dictionary construction, pair 2 | 18,016,256,000 | 17,892,179,968 | −124,076,032 |
| Peak resident through first solve, pair 1 | 18,016,370,688 | 17,892,327,424 | −124,043,264 |
| Peak resident through first solve, pair 2 | 18,016,366,592 | 17,892,257,792 | −124,108,800 |
| Validator construction peak | 144,272,784 | 16,253,440 | −128,019,344 |
| Retained shared dictionary structures | 17,814,660,866 | 17,814,660,866 | 0 |
| Retained validator | 144,272,720 | 15,360,024 | −128,912,696 |
| Retained dictionary including validator | 17,958,933,586 | 17,830,020,890 | −128,912,696 |
| Incremental retained board | 1,472 | 5,120 | +3,648 |

The cache holds 233,254,549 patterns, 287,119,173 anagram groups and 287,375,581 word indices. Their three payloads alone occupy 17,801,461,428 bytes once loaded. The packed validator saves 128,912,696 retained bytes per dictionary. Each prepared board adds 3,600 bytes for its four 15×15 ID arrays and 48 bytes for the two lookup references and shared allocation control blocks.

Both pairs produced identical owned-byte totals. The first-solve RSS savings differed by 65,536 bytes between the two pairs.
