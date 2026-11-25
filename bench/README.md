# Paired validator benchmark

Run from the repository root with Python 3.10+ and the C++17 toolchain.

| Directory | What belongs here |
| --- | --- |
| [validator_comparison/](validator_comparison/) | Paired timing of the original and packed validators, including the mass comparison of their results. |
| [window_counters/](window_counters/) | Qualification and timing of window counters on the same prepared board. |
| [memory/](memory/) | Allocation tracing and memory comparisons for the first solve. |
| [measurement_support/](measurement_support/) | Input handling, cache selection and solver code used by multiple measurements. |
| [corpus/](corpus/) | Recorded-game import, replay, sampling and export of benchmark positions. |

Each measurement owns its runner and native worker. Python code shared between measurements belongs in `measurement_support/`. `make check-bench-imports` checks this rule and runs as part of `make test`. Run the Python commands below as modules from the repository root.

## Compare window counters on prepared boards

`make -s window-benchmark` qualifies and times `packed` and `one-gap` on the small fixture. Use `--variants BASELINE CHALLENGER` to select a different pair. Each CSV row records both solve times in microseconds, their difference (baseline minus challenger), the run order, stage, directional counts, input fingerprints, compiler flags, worker fingerprint and CPU affinity.

To compare counters on one position, pass a board CSV and its rack. The board path is relative to the working directory. The output uses the board filename stem as the case name and `single` as the stage.

```sh
make -s window-benchmark BENCH_ARGS='--board bench/fixtures/paired/crossings.csv --rack RT'
```

For batches, pass `--cases` with a CSV headed `case,stage,board,rack`. Each case name must be distinct. Board paths are relative to the manifest and racks contain one to seven uppercase tiles in any order. Existing paired manifests work too. Window timing reads only these four fields.

The qualifier compares each counter with place-and-restore on every measured position. It checks every empty square's allowed letters and compares candidate coordinates, words and decisions with duplicates retained in each direction. It also checks the restored rows and prepared grid identities, and the packed counter's lookup counts. The timing worker loads the same dictionary and pattern table once and prepares the board before each pair. Each pair runs two warm-up rounds, then one measured round. Both counters solve the same prepared board in the assigned order in all three rounds. The clock covers rack-subset generation, both directional counts and the final view swap. The runner checks both release counts against qualification before writing the CSV. Within each stage and repetition, successive cases alternate which counter goes first. Each case also reverses order on its next repetition.

The recorded runs used logical CPU 2 on an i7-6700K. Select an available CPU on another host. The replay command verifies and exports the six pilot positions using the full project dictionary.

```sh
flags='-std=c++17 -O2 -march=x86-64 -Wall -Wextra -Iinclude'
taskset -c 2 make -s window-benchmark CXX=clang++ CXXFLAGS="$flags" \
    BENCH_ARGS='--variants packed one-gap --repetitions 20' > bench/results/window-fixture.csv
python3 -m bench.corpus.sample_turns --verify bench/fixtures/pilot/corpus.json --window-dir build/window-pilot
taskset -c 2 make -s window-benchmark CXX=clang++ CXXFLAGS="$flags" \
    BENCH_ARGS='--variants packed one-gap --repetitions 20 --cases build/window-pilot/cases.csv --dictionary data/dictionaries/sample_dictionary.txt' \
    > bench/results/window-pilot.csv
```

To add a counter, define its window callable beside `PackedCounter` and register `measure_solve<YourCounter>` under a name in `window_variant`. The qualifier and worker compile that registry. The production entry calls the packed counter directly. For workload profiles, shared traversal records each window and group visit. `WindowMasks::accepts` records an individual check at entry. A counter with its own per-word acceptance test should record one `trace_individual_check()` per word it checks. [Counter implementations and registry](../src/rack_oriented_solver.cpp)

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

### Filter empty squares by perpendicular neighbors

`packed` scans the window from left to right. `empty-bits` visits the same gaps in the same order by iterating the set bits of the window’s empty-square mask. `neighbor-bits` first intersects that mask with the occupancy union of the adjacent rows in the active board view. A skipped square allows every letter. A square with a neighbor still gets its full perpendicular-check set. All three build masks on the first matching group and use the same per-word acceptance test.

| Paired comparison | Baseline mean (µs/solve) | Challenger mean (µs/solve) | Paired saving (µs/solve) | 95% CI on saving |
| --- | ---: | ---: | ---: | ---: |
| Packed scan → empty bits | 394.42 | 392.60 | +1.82 | −0.13 to +3.87 |
| Packed scan → neighbor bits | 392.72 | 371.39 | +21.32 | +17.76 to +25.27 |
| Empty bits → neighbor bits | 402.34 | 382.92 | +19.43 | +15.09 to +23.81 |

Each row comes from its own paired run. Positive savings favor the challenger. Empty-bit iteration alone tied the packed scan at this precision. Filtering saved 4.8% against unfiltered bit iteration. The direct saving was 17.63 µs on the six pilot games (95% CI 10.68 to 24.57) and 19.88 µs on the 24 additional development games (14.89 to 24.99). An earlier build at `278b049` saved 9.19 µs in the direct comparison on the same games. [Scan vs. empty-bit pairs](results/window-packed-empty-bits.csv.gz) · [Scan vs. filtered pairs](results/window-packed-neighbor-bits.csv.gz) · [Direct filter pairs](results/window-empty-bits-neighbor-bits.csv.gz)

The full-dictionary run used Clang 18.1.3 at `-O2 -march=x86-64` on logical CPU 2 of the i7-6700K. Each pairing has 40 repetitions per game, 20 in each order. Each game contributes its mean paired difference. Confidence intervals resample whole games within each stage (sorted by case name, 20,000 draws, seed 15) and weight early, middle and late equally. The 30 development games include the six pilot games and 24 separate Quackle games. The timing covers rack subsets, both views and restoration on a prepared board.

The separate trace counted calls during mask construction. The last column counts only windows that reached at least one matching rack group. The 14,578 windows with no matching group already avoided construction in all three variants. Including them, 17,003 of 23,762 visited windows made zero filtered lookups.

| Stage | Packed or empty-bit lookups | Filtered lookups | Calls eliminated | Matched windows with zero filtered lookups |
| --- | ---: | ---: | ---: | ---: |
| Early | 9,151 | 3,344 | 5,807 | 869 / 2,652 |
| Middle | 11,127 | 5,111 | 6,016 | 890 / 3,469 |
| Late | 8,361 | 4,621 | 3,740 | 666 / 3,063 |
| **All** | **28,639** | **13,076** | **15,563 (54.3%)** | **2,425 / 9,184 (26.4%)** |

Filtering also removes the skipped squares’ descriptors from the per-word loop. All three constructors still visited 49,421 groups, produced 72,530 candidate decisions and ran 40,724 individual word checks. Across and down totals are available for each stage. [Development workload and lookup counts](results/window-workload.json)

The production solver translation unit’s `.text` section matched the pre-experiment build byte for byte with the measured flags. In the linked worker, both bit constructors select the lowest set bit before calling `Board::perpendicular_check_set`. The filtered constructor intersects the mask with the row union and jumps over that loop when the result is zero.

These commands reproduce the timings and the separate lookup profile from the repository root.

```sh
flags='-std=c++17 -O2 -march=x86-64 -Wall -Wextra -Iinclude'
window_tools=$(make -s window-tools CXX=clang++ CXXFLAGS="$flags")
python3 -m bench.window_counters.window_games --export development build/window-development
for pair in 'packed empty-bits' 'packed neighbor-bits' 'empty-bits neighbor-bits'; do
    set -- $pair
    taskset -c 2 python3 -m bench.window_counters.window_benchmark --build-dir "$window_tools" --variants "$1" "$2" \
        --cases build/window-development/cases.csv --dictionary data/dictionaries/sample_dictionary.txt \
        --repetitions 40 > "build/window-$1-$2.csv" || break
done
python3 -m bench.window_counters.window_profile > build/window-workload.json
```

### Reuse perpendicular-check sets across overlapping windows

On 30 development games, `row-cache` saved **8.81 µs per complete solve (2.25%)** against `packed`. Both counters scan every gap when a window first reaches a matching group. The cache stores the first result for each queried column in the current row and board view. A separate validity bit lets it reuse an empty set. The timed worker initializes the validity mask for each row and tests it on every request. [Raw paired solves](results/window-packed-row-cache.csv.gz)

| Stage | Packed (µs/solve) | Row cache (µs/solve) | Paired saving (µs/solve) |
| --- | ---: | ---: | ---: |
| Early | 339.26 | 329.83 | 9.44 |
| Middle | 491.85 | 478.93 | 12.92 |
| Late | 344.89 | 340.81 | 4.08 |
| **Equal-stage mean** | **392.00** | **383.19** | **8.81** |

The 95% game-level interval for the saving was **5.43 to 12.12 µs**. The six pilot games saved 13.10 µs (95% CI 10.20 to 16.01), and the 24 additional games saved 7.74 µs (4.08 to 11.59). Each game contributed 40 pairs, 20 in each order, on logical CPU 2 of the i7-6700K with Clang 18.1.3 at `-O2 -march=x86-64` and the full project dictionary. Intervals resample mean paired differences by game within each stage, sorted by case name, with 20,000 draws and seed 15. The earlier implementation at `1ca4ce2` saved 11.14 µs on the same games (95% CI 6.21 to 16.36). Both counters agreed with place-and-restore on all 72,530 candidate decisions and all release runs returned the qualified directional counts.

The separate diagnostic run counted **23,539 hits on 28,639 requests (82.2%)**. Each miss was the first request for its square in that row and view. Both counters visited the same 49,421 matching groups and performed the same 40,724 individual word checks. Reuse removes repeated lookups during mask construction, which accounts for one part of the solve. [Per-row distributions by stage and direction](results/window-workload.json)

| Stage | Requests | Distinct squares summed over rows | Mean distinct squares per row | Cache hits | Rows with zero requests |
| --- | ---: | ---: | ---: | ---: | ---: |
| Early | 9,151 | 1,434 | 4.78 | 7,717 | 136 / 300 |
| Middle | 11,127 | 1,816 | 6.05 | 9,311 | 66 / 300 |
| Late | 8,361 | 1,850 | 6.17 | 6,511 | 31 / 300 |
| **All** | **28,639** | **5,100** | **5.67** | **23,539** | **233 / 900** |

To repeat the comparison and collect the untimed profile, run these commands from the repository root.

```sh
flags='-std=c++17 -O2 -march=x86-64 -Wall -Wextra -Iinclude'
window_tools=$(make -s window-tools CXX=clang++ CXXFLAGS="$flags")
python3 -m bench.window_counters.window_games --export development build/window-development
taskset -c 2 python3 -m bench.window_counters.window_benchmark --build-dir "$window_tools" \
    --variants packed row-cache --cases build/window-development/cases.csv \
    --dictionary data/dictionaries/sample_dictionary.txt --repetitions 40 > build/window-packed-row-cache.csv
python3 -m bench.window_counters.window_profile > build/window-workload.json
```

### Mask construction and stopping impossible windows

The first three comparisons isolate construction timing, ending the scan, and ending the group search. The fourth measures both early exits together against packed. `eager` constructs the mask as soon as the dictionary contains the window’s word pattern. `packed` waits for the first matching rack subset. `stop-gap` keeps that lazy construction and ends the scan at the first empty perpendicular-check set. `stop-window` also ends the search for matching groups in that window. A group whose words all fail can still be followed by a group that produces a move.

| Control → challenger | Control µs/solve | Challenger µs/solve | Paired saving µs/solve (95% CI) | Raw pairs |
| --- | ---: | ---: | ---: | --- |
| `packed` → `eager` | 399.16 | 399.44 | −0.28 (−3.59 to +2.99) | [CSV](results/window-packed-eager.csv.gz) |
| `packed` → `stop-gap` | 407.24 | 403.88 | +3.35 (−3.37 to +8.11) | [CSV](results/window-packed-stop-gap.csv.gz) |
| `stop-gap` → `stop-window` | 404.29 | 361.88 | +42.40 (+33.53 to +51.32) | [CSV](results/window-stop-gap-stop-window.csv.gz) |
| `packed` → `stop-window` | 403.09 | 349.14 | +53.94 (+43.57 to +63.86) | [CSV](results/window-packed-stop-window.csv.gz) |

The intervals for eager construction and for stopping only the scan include zero, leaving both tied with packed at the measured precision. Clang inlined the eager group visitor and emitted a call to the packed visitor in both matching loops. Ending the group search saved 42.40 µs per solve against its immediate control. This saving was 33.23 µs on the six pilot games and 44.70 µs on the other 24 development games. The direct comparison with packed saved 13.4%. Each row uses its own paired differences before rounding.

The four runs used the full project dictionary, Clang 18.1.3 with `-O2 -march=x86-64`, and logical CPU 2 of the i7-6700K. Each row summarizes 1,200 pairs over 30 development games, with two warm-ups per counter and 20 measured pairs per game in each order. For each interval, games are sorted by case name within stage. The bootstrap resamples their paired means within stage, gives each stage equal weight, and uses 20,000 draws with seed 15. Every release observation returned the qualified across and down counts.

The untimed trace counted one mask per construction and one lookup per queried gap. The 5,732 windows whose pattern had no dictionary match never reached construction. The other 18,030 windows broke down as follows. [Development diagnostics by stage](results/window-workload.json)

| Window outcome | Windows | Masks, eager / lazy | Eager lookups | Packed lookups | Stop-gap or stop-window lookups |
| --- | ---: | ---: | ---: | ---: | ---: |
| At least one accepted word | 3,725 | 3,725 / 3,725 | 9,621 | 9,621 | 9,621 |
| All matching words rejected | 5,459 | 5,459 / 5,459 | 19,018 | 19,018 | 14,674 |
| Word pattern with no matching rack subset | 8,846 | 8,846 / 0 | 44,110 | 0 | 0 |
| **Total** | **18,030** | **18,030 / 9,184** | **72,749** | **28,639** | **24,295** |

Stopping construction skipped **4,344 lookups** within the 3,246 matched windows with an empty check set. Stopping the group search there also avoided **43,613 subset probes** and **23,280 merge comparisons**. One probe is a search for one rack subset. One merge comparison examines the current pair of group and subset keys. The trace continued both searches to identify the 17,048 skipped groups and their 27,401 word IDs. All 72,530 logical candidate decisions, including those rejections, matched place-and-restore with multiplicity in each direction. All four counters still performed 40,724 individual word checks. The linked release worker tests the visitor result and branches to the next window in both loops.

To reproduce the four comparisons and the separate diagnostic profile, run from the repository root.

```sh
flags='-std=c++17 -O2 -march=x86-64 -Wall -Wextra -Iinclude'
window_tools=$(make -s window-tools CXX=clang++ CXXFLAGS="$flags")
python3 -m bench.window_counters.window_games --export development build/window-development
for pair in 'packed eager' 'packed stop-gap' 'stop-gap stop-window' 'packed stop-window'; do
    set -- $pair
    taskset -c 2 python3 -m bench.window_counters.window_benchmark --build-dir "$window_tools" \
        --variants "$1" "$2" --cases build/window-development/cases.csv \
        --dictionary data/dictionaries/sample_dictionary.txt --repetitions 40 > "build/window-$1-$2.csv" || break
done
python3 -m bench.window_counters.window_profile > build/window-workload.json
```

### Pruning with the rack and matching rack subsets

Using the actual rack saved **24.14 µs per solve** against the matched full-alphabet control (95% CI 19.04 to 29.35). The saving was 15.84 µs on the six pilot games and 26.22 µs on the 24 other development games. The full rack policy saved **33.13 µs** against ticket 05’s `stop-window` and **85.11 µs** against `packed`.

Checking every matched subset’s alphabet cost 5.70 µs relative to `rack`. Adding the repeated-constraint capacity check to that test saved 4.01 µs in its own paired run. When both subset tests were compared directly with `rack`, they cost 3.80 µs per solve, a tie at this precision. Applying both tests only to multiword groups cost 5.16 µs against `rack`.

`all-letters` and `rack` use the same window search. The control supplies A–Z. The rack variant builds its alphabet once after the start clock and rack-subset generation, then reuses it in both views. Both counters intersect each gap’s perpendicular-check set with the supplied alphabet. An empty intersection rules out the window. A gap that admits the entire alphabet drops out. If every gap drops out, the whole group counts without fetching a candidate word.

`subset` repeats the intersection with the matched subset's alphabet on the remaining gaps. `subset-capacity` also counts tile occurrences when several gaps have the same eligible letters. A group with too few eligible tiles contributes zero and the search continues. `subset-multi` applies both subset checks only to groups of two or more words, so its direct comparison with `subset-capacity` measures the cost of enabling them on singletons.

| Control → challenger | Control µs/solve | Challenger µs/solve | Paired saving µs/solve (95% CI) | Raw pairs |
| --- | ---: | ---: | ---: | --- |
| `packed` → `all-letters` | 410.05 | 344.46 | +65.59 (+52.08 to +80.13) | [CSV](results/window-packed-all-letters.csv.gz) |
| `stop-window` → `all-letters` | 341.67 | 335.62 | +6.05 (+3.46 to +8.84) | [CSV](results/window-stop-window-all-letters.csv.gz) |
| `all-letters` → `rack` | 340.51 | 316.37 | +24.14 (+19.04 to +29.35) | [CSV](results/window-all-letters-rack.csv.gz) |
| `packed` → `rack` | 397.55 | 312.44 | +85.11 (+69.49 to +100.35) | [CSV](results/window-packed-rack.csv.gz) |
| `stop-window` → `rack` | 345.41 | 312.28 | +33.13 (+27.32 to +39.12) | [CSV](results/window-stop-window-rack.csv.gz) |
| `packed` → `subset` | 399.01 | 323.79 | +75.22 (+61.04 to +89.25) | [CSV](results/window-packed-subset.csv.gz) |
| `rack` → `subset` | 323.78 | 329.48 | −5.70 (−9.15 to −1.74) | [CSV](results/window-rack-subset.csv.gz) |
| `packed` → `subset-capacity` | 404.90 | 321.59 | +83.31 (+67.42 to +100.20) | [CSV](results/window-packed-subset-capacity.csv.gz) |
| `subset` → `subset-capacity` | 393.59 | 389.59 | +4.01 (+1.23 to +6.82) | [CSV](results/window-subset-subset-capacity.csv.gz) |
| `rack` → `subset-capacity` | 337.43 | 341.24 | −3.80 (−8.55 to +0.64) | [CSV](results/window-rack-subset-capacity.csv.gz) |
| `packed` → `subset-multi` | 402.15 | 322.79 | +79.36 (+64.40 to +94.25) | [CSV](results/window-packed-subset-multi.csv.gz) |
| `rack` → `subset-multi` | 314.67 | 319.83 | −5.16 (−7.37 to −2.93) | [CSV](results/window-rack-subset-multi.csv.gz) |
| `subset-multi` → `subset-capacity` | 314.98 | 315.36 | −0.38 (−2.61 to +1.72) | [CSV](results/window-subset-multi-subset-capacity.csv.gz) |

Each row uses 1,200 full-dictionary pairs on the 30 development games, including 13 racks with repeated letters. Clang 18.1.3 compiled the release worker for portable x86-64 at `-O2`. The pairs ran on logical CPU 2 of the i7-6700K, with two warm-ups per counter and 20 measured pairs per game in each order. The interval resamples paired game means within stage, sorted by case name, with 20,000 draws and seed 15. Early, middle and late games have equal stage weight. Each saving comes from its own paired run. Positive savings favor the challenger.

The untimed qualifier matched all 72,530 candidate decisions against place-and-restore for every counter, including rejected groups skipped by release. Each timed observation returned the qualified across and down counts. The table counts a word read when the solver has to fetch a candidate to check its gap letters. The last column measures avoided reads against the 40,724 in both `packed` and `stop-window`. [Diagnostics by stage and group size](results/window-workload.json)

| Counter | Windows ruled out | Groups rejected by window | Groups rejected by subset (disjoint / capacity) | Satisfied gaps dropped (window / group) | Word reads | Reads avoided |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| `stop-window` | 3,246 | 20,294 | 0 / 0 | 0 / 0 | 40,724 | 0 |
| `all-letters` | 3,246 | 20,294 | 0 / 0 | 13,015 / 0 | 32,351 | 8,373 |
| `rack` | 4,450 | 27,420 | 0 / 0 | 12,209 / 0 | 21,624 | 19,100 |
| `subset` | 4,450 | 27,420 | 4,974 / 0 | 12,209 / 1,466 | 14,071 | 26,653 |
| `subset-capacity` | 4,450 | 27,420 | 4,974 / 1,812 | 12,209 / 1,466 | 11,382 | 29,342 |
| `subset-multi` | 4,450 | 27,420 | 1,653 / 680 | 12,209 / 417 | 16,277 | 24,447 |

Relative to `all-letters`, the rack ruled out 1,204 additional windows containing 7,126 matching groups and 10,716 words. Another 11 words became bulk accepts, bringing the extra reads avoided by rack information to 10,727. The full-alphabet control already accepted 8,373 words in bulk. Of the 12,209 gaps dropped during window construction, 19 had a perpendicular-check set narrower than the full alphabet. Window removals count a gap once per mask built. Subset removals count a gap once per group checked. Both include gaps dropped before a later check fails. The rack stopped construction earlier, reaching fewer unrestricted gaps than the full-alphabet control. Groups rejected by the window include the first matched group and every skipped match.

Subset checks ran on 14,506 groups. Of those, 9,080 were singletons. On singletons the subset alphabet rejected 3,321, tile capacity rejected another 1,132, and 442 passed every remaining gap in bulk. That spared 4,895 reads and still left 4,185 singletons requiring a word check. Enabling the subset checks on those singletons cost 0.38 µs per complete solve (95% CI from a 1.72 µs saving to a 2.61 µs cost), a tie at this precision. The comparison includes the branch that selects singletons.

An earlier worker at `9c0942c` used the same rack and subset algorithms. The subset alphabet tied `rack` there, adding capacity cost 5.06 µs, and enabling both tests on singletons cost 4.37 µs. Checking only multiword groups saved 5.00 µs against `rack` in that earlier run. The two builds made the same pruning decisions. In the linked group visitors the instruction counts match and the code and constant addresses moved. Both runs found a saving for whole-rack pruning against `stop-window`. The smaller subset effects varied between builds and runs.

The traversal fixtures put two identical crossings in one window. With the rack RRSTTT, the RRS group has no eligible tile and RST has only one T for two gaps. The later RTT group accepts ATTR and rejects ATRT, and TTT passes in bulk. Both the binary probes and the sorted merge produced these outcomes in the trace and the qualified directional totals in release.

To reproduce the comparisons and the diagnostic profile from the repository root, run

```sh
flags='-std=c++17 -O2 -march=x86-64 -Wall -Wextra -Iinclude'
window_tools=$(make -s window-tools CXX=clang++ CXXFLAGS="$flags")
python3 -m bench.window_counters.window_games --export development build/window-development
for pair in 'all-letters rack' 'stop-window all-letters' 'packed all-letters' 'stop-window rack' 'packed rack' 'rack subset' 'subset subset-capacity' \
            'subset-multi subset-capacity' 'packed subset' 'packed subset-capacity' \
            'packed subset-multi' 'rack subset-multi' 'rack subset-capacity'; do
    set -- $pair
    taskset -c 2 python3 -m bench.window_counters.window_benchmark --build-dir "$window_tools" \
        --variants "$1" "$2" --cases build/window-development/cases.csv \
        --dictionary data/dictionaries/sample_dictionary.txt --repetitions 40 > "build/window-$1-$2.csv" || break
done
python3 -m bench.window_counters.window_profile > build/window-workload.json
```

### Constrained gaps and bulk counting

Dropping unrestricted gaps while checking every candidate saved **8.74 µs/solve** against `stop-window` (95% CI 5.51 to 12.07). Bulk counting tied `constrained-only`. Its paired saving was **-0.29 µs/solve** (95% CI -2.79 to +2.22). On the six pilot games the paired savings were +8.88 and -5.52 µs/solve, respectively. On the other 24 development games they were +8.71 and +1.01 µs/solve.

| Paired comparison | Control µs/solve | Challenger µs/solve | Paired saving µs/solve (95% CI) | Raw pairs |
| --- | ---: | ---: | ---: | --- |
| `stop-window` → `constrained-only` | 365.21 | 356.47 | +8.74 (+5.51 to +12.07) | [CSV](results/window-constrained-gaps/stop-window-constrained-only.csv.gz) |
| `constrained-only` → `all-letters` | 337.13 | 337.42 | -0.29 (-2.79 to +2.22) | [CSV](results/window-constrained-gaps/constrained-only-all-letters.csv.gz) |
| `packed` → `constrained-only` | 406.05 | 350.77 | +55.28 (+44.81 to +65.51) | [CSV](results/window-constrained-gaps/packed-constrained-only.csv.gz) |
| `packed` → `all-letters` | 399.32 | 345.78 | +53.54 (+43.26 to +63.68) | [CSV](results/window-constrained-gaps/packed-all-letters.csv.gz) |
| `packed` → `stop-window` | 394.86 | 346.41 | +48.45 (+38.47 to +58.15) | [CSV](results/window-constrained-gaps/packed-stop-window.csv.gz) |

All five comparisons used the same Clang 18.1.3 worker at `-O2 -march=x86-64`, the full dictionary and logical CPU 2 on the i7-6700K. Each pair ran 40 times per game, 20 in each order, on the 30 development games. The interval resamples paired game means within each stage (20,000 draws, seed 15) and weights early, middle and late equally. Positive savings favor the challenger.

| Counter | Descriptors retained | Descriptors per built window | Individual candidate checks | Word-ID/pointer fetches skipped vs. `stop-window` |
| --- | ---: | ---: | ---: | ---: |
| `packed` | 28,639 | 3.12 | 40,724 | 0 |
| `stop-window` | 21,049 | 2.29 | 40,724 | 0 |
| `constrained-only` | 8,034 | 0.87 | 40,724 | 8,373 |
| `all-letters` | 8,034 | 0.87 | 32,351 | 8,373 |

The denominator is the 9,184 windows that built a mask. The original empty-square count selects the rack subsets even when every descriptor drops out. All three stopping counters made 24,295 square lookups and rejected the same 3,246 impossible windows. Each stopped before storing the first empty perpendicular-check set and skipped the remaining matching groups. The totals include descriptors written before that stop. The fixed array still has room for seven descriptors. The qualifier matched all 72,530 candidate decisions, including multiplicity, in both directions. Every release observation returned its qualified counts. [Diagnostics by stage](results/window-workload.json)

The linked worker shows where the candidate word reads disappear. With all gaps unrestricted, `stop-window` loads each word ID and dictionary pointer because its descriptor count is positive. The three counters share a scan that tests the lookup result directly. The retention test in the two filtered counters compares it with the constant A–Z mask. The historical rack comparison above used the runtime alphabet control from `9cd995d`. In `constrained-only`, Clang tests the retained count inside the candidate loop before loading the ID or pointer. Dropping the descriptors therefore skips those loads for 8,373 candidates while still iterating over each one. `all-letters` skips the entire loop for the same 7,487 groups, including 6,693 singletons. Bulk counting removes a further 8,373 iterations and zero additional word-ID or dictionary-pointer loads in this build.

To rerun the five comparisons and the separate diagnostic profile from the repository root, use

```sh
flags='-std=c++17 -O2 -march=x86-64 -Wall -Wextra -Iinclude'
window_tools=$(make -s window-tools CXX=clang++ CXXFLAGS="$flags")
python3 -m bench.window_counters.window_games --export development build/window-development
for pair in 'stop-window constrained-only' 'constrained-only all-letters' 'packed constrained-only' \
            'packed all-letters' 'packed stop-window'; do
    set -- $pair
    taskset -c 2 python3 -m bench.window_counters.window_benchmark --build-dir "$window_tools" \
        --variants "$1" "$2" --cases build/window-development/cases.csv \
        --dictionary data/dictionaries/sample_dictionary.txt --repetitions 40 > "build/gaps-$1-$2.csv" || break
done
python3 -m bench.window_counters.window_profile > build/window-workload.json
```

### Profile the development workload and export reserved games

Run the untimed qualifier on the development set with the full project dictionary. It checks each named counter against the place-and-restore validator. For each game, counter and direction, the words in visited groups plus the words in any skipped groups must account for all of the validator’s candidate records.

```sh
mkdir -p build
python3 -m bench.window_counters.window_profile > build/window-workload.json
cmp build/window-workload.json bench/results/window-workload.json
python3 -m bench.window_counters.window_games | cmp - bench/fixtures/window-game-sets.csv
python3 -m bench.window_counters.window_games --export development build/window-development
```

Each of the 30 development games contributes one turn. Within each stage, development uses the two pilot games and the first eight games from the saved Quackle corpus. PGO uses the next 16 games per stage. The held-out set takes the last 32 per stage for confirmation after finalist selection. Selection uses saved corpus order and stage only. The exporter checks the frozen case identities and rejects repeated game IDs or source records across the three lists. Use `--export pgo build/window-pgo` for profile training and `--export held-out build/window-held-out` for final comparisons. Each export writes the selected boards and a `cases.csv` for the window benchmark. [Frozen game lists](fixtures/window-game-sets.csv)

| Stage | Windows visited | No dictionary match | Pattern with zero matching groups | Visited groups | Candidate words | Packed individual checks | One-gap individual checks |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Early | 6,408 | 751 | 3,005 | 13,308 | 20,211 | 12,228 | 11,783 |
| Middle | 9,001 | 2,326 | 3,206 | 23,118 | 34,891 | 19,512 | 18,622 |
| Late | 8,353 | 2,655 | 2,635 | 12,995 | 17,428 | 8,984 | 8,125 |

The two zero-group columns together cover every visited window with zero candidates. Each group visit contributes its full word range to candidate words, including words rejected by an impossible gap. Individual checks count actual calls to the per-word acceptance test. Windows with exactly one empty square accounted for 467 early candidates (2.3%), 986 middle (2.8%) and 1,047 late (6.0%). Packed already ruled out some of these in bulk. The one-gap shortcut avoided 445, 890 and 859 additional individual checks, respectively.

The JSON gives exact frequency tables by stage and counter for empty squares, constrained gaps and candidates per window. It keeps windows with matching groups, patterns with no matching rack subset and patterns with no dictionary match separate. A constrained gap is an empty square whose perpendicular-check set permits fewer than 26 letters, including zero. The qualifier inspects those squares after the traced solve, so these diagnostic lookups preserve the baseline lookup check. The `groups_by_size` table gives visits, candidate words and individual checks for each group size. [Full development profile](results/window-workload.json)

## Replay a recorded game

The saved game contains all 20 placements and pre-move racks from Alec Sjoholm vs. Cesar Del Solar. Turns are numbered from one in playing order. The solver can use turns **2–6, 8, 10–16 and 18–20**. Turn 1 opens the board. On turns 7, 9 and 17 the rack contains a blank. Replay still applies those moves so later boards contain the assigned letters. [Recorded racks and moves](fixtures/replay/sjoholm-del-solar.csv) · [Original Macondo game record](https://github.com/domino14/macondo/blob/9fe4fe8cdf3b4e6cbcf42be5f205d463510c2e72/puzzles/testdata/well_played_game.gcg)

```sh
python3 -m bench.corpus.replay_game
python3 -m bench.corpus.replay_game --turn 18 --board-csv /tmp/turn-18.csv
python3 -m bench.corpus.replay_game --turn 18 | cmp - bench/fixtures/replay/turn-18.json
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
python3 -m bench.corpus.sample_turns --seed 10 | cmp - bench/fixtures/pilot/corpus.json
python3 -m bench.corpus.replay_game --game bench/fixtures/pilot/games/6zTBhes669.csv \
    --dictionary data/dictionaries/sample_dictionary.txt --turn 25
python3 -m bench.corpus.sample_turns --benchmark-dir /tmp/scrabble-pilot > /tmp/scrabble-pilot.json
```

From a clean checkout, the following command verifies every saved board, rack and stage using the bundled 252-word dictionary. It covers every word played or formed in these six games. Each `game` path is relative to the input games manifest. To use a different collection or dictionary, pass `--games` or `--dictionary`. [Replay dictionary](fixtures/pilot/words.txt)

```sh
python3 tests/sample_turns_test.py SampleTurnsTest.test_replays_every_saved_case_with_the_fixture_dictionary
```

Use `--window-dir` to export the sampled positions directly for window timing, including a sample of one turn. The `--benchmark-dir` export writes solver CSV boards and `cases.csv` for the paired validator runner below. For each case it takes the next different board in the same stage whose rack has at least two tiles, wrapping at the end. For the new-rack workload it removes the last recorded tile from that board's rack. A one-tile rack is still eligible for the first-board workload. Each case needs a suitable companion for export. Use the same dictionary for sampling and timing.

## Replay or resize the mass-scale corpus

The saved corpus contains **10,000 distinct board-and-rack pairs from 10,000 recorded Quackle self-play games**. Seed 11 selected 3,334 early, 3,333 middle and 3,333 late positions. Each case includes its pre-move board, actual rack, turn number, rank among eligible turns, stage, source URL and sample dictionary SHA-256 (`559faa1e0296166f8f4c6074b71211c045202accb6920424aaa0758cc7bce9cd`). [Ordered cases](fixtures/mass/corpus.json.gz) · [Game manifest and recordings](fixtures/mass/games.tar.gz)

With the project dictionary in `data/dictionaries/sample_dictionary.txt`, these commands reproduce the selection and compare every case in order, including the dictionary fingerprint. The optional export gives both validators the same ordered manifest.

```sh
mkdir -p build/mass-games
tar -xzf bench/fixtures/mass/games.tar.gz -C build/mass-games
python3 -m bench.corpus.sample_turns --games build/mass-games/games.csv \
    --verify bench/fixtures/mass/corpus.json.gz --benchmark-dir build/mass-paired
```

Set `--size` to the number of distinct games required. For example, the following selects 12,000 positions from the saved pool. The sampler gives leftover cases to the earlier stages. It compares boards as the solver reads them and racks by sorted letters, retaining every copy. If a pair repeats, it tries another eligible turn or game. If the pool cannot fill the quotas, the command exits with an error.

```sh
python3 -m bench.corpus.sample_turns --games build/mass-games/games.csv \
    --seed 11 --size 12000 > build/corpus-12000.json
```

The bundled pool contains **12,592 games**. For larger samples, supply a larger `game,source` manifest with one recording per game. Paths are relative to the manifest. To inspect an individual selection, pass its game path and turn to `python3 -m bench.corpus.replay_game` with the same dictionary.

The recordings come from Kevin McElwee's tile-value experiment. Of the selected games, 3,645 used traditional scoring and 6,355 used alternate tile values. The importer keeps complete games containing only placements and replays every move against the sample dictionary. This excludes games with exchanges or passes. The benchmark consumes board letters and actual racks. The tile values influence which moves Quackle played. [Pinned original game records](https://github.com/kmcelwee/scrabble/tree/b4c0ccf7c6db50399a3f867cb359b45c861292f0/data)

To rebuild the source pool from the original archive, run the following commands. The importer checks the archive SHA-256 before reading the recordings.

```sh
curl -fL https://codeload.github.com/kmcelwee/scrabble/tar.gz/b4c0ccf7c6db50399a3f867cb359b45c861292f0 \
    -o build/quackle-games.tar.gz
python3 -m bench.corpus.import_quackle_games build/quackle-games.tar.gz build/imported-games
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
python3 -m bench.corpus.sample_turns --verify bench/fixtures/pilot/corpus.json --benchmark-dir build/timing-pilot
dictionary=data/dictionaries/sample_dictionary.txt
for condition in hit miss; do
    pilot=bench/results/pilot-$condition.csv
    pairs=bench/results/mass-$condition.csv
    cases=build/timing-$condition
    taskset -c 2 python3 -m bench.validator_comparison.paired_benchmark --dictionary "$dictionary" \
        --condition "$condition" --repetitions 1 --cases build/timing-pilot/cases.csv > "$pilot" || break
    python3 -m bench.validator_comparison.mass_comparison --condition "$condition" --pilot "$pilot" --benchmark-dir "$cases" || break
    taskset -c 2 python3 -m bench.validator_comparison.paired_benchmark --dictionary "$dictionary" \
        --condition "$condition" --repetitions 1 --cases "$cases/cases.csv" > "$pairs" || break
    python3 -m bench.validator_comparison.mass_comparison --condition "$condition" --pilot "$pilot" --report "$pairs" || break
done
```

For each cache condition, the pilot estimates the variance of **original minus packed** first-board milliseconds within each stage. Let $T$ be 1% of the equally weighted original mean, and $s_h^2$ the sample variance for stage $h$. For $m$ independent games per stage, the estimated variance of the equally weighted saving is $\sum_h s_h^2/(9m)$. Set $m$ to the ceiling of $1.96^2 \sum_h s_h^2/(9T^2)$ with a minimum of two games for the order crossover. This sizes the verified 10,000-game starting corpus independently for hits and misses. Take the first $m$ saved cases of each stage in corpus order and use the same-stage companion and additional rack from the benchmark export. The six pilot games are separate from the full estimates.

The report uses 20,000 bootstrap draws with seed 15. Each draw samples whole paired game differences with replacement within each stage at its observed size, then averages the three stage means equally. For reuse, the measured game is the companion that provided the board. If a one-tile rack caused the exporter to reuse a companion, average its repeat timings before computing means, ranges and intervals so the game receives one vote. The 2.5th and 97.5th percentiles give the interval. The reported spread is the observed range of paired game differences. The final half-width is checked against the full-run original mean. Pilot sizing estimates precision from just two games per stage, so the final interval can miss its target. Break-even uses the ceiling of the observed first-board premium divided by the mean saving per additional board or rack. The report distinguishes intervals spanning zero from reliably positive and reliably negative reuse savings.

## Fixture run

Revision `691c1b0` ran 20 pairs per cache condition in manifest order. Both workers used the same executable, built with Ubuntu Clang 18.1.3 and `-std=c++17 -O2 -Wall -Wextra -Iinclude`, the default glibc 2.39 allocator, and logical CPU 2 of an Intel Core i7-6700K under x86-64 Linux on WSL2. Inputs and temporary caches were on ext4 with the pre-read policy described above.

```sh
taskset -c 2 python3 -m bench.validator_comparison.paired_benchmark --repetitions 20 > bench/results/paired-fixture.csv
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
