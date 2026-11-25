# Window-counter finalists

**Portable C++17 advances three counters.** `rack-neighbor-cache`, `rack-all-checks` and `rack-neighbor` tied in all three direct development comparisons. Adding the row cache to `rack-neighbor` cost 2.04 µs/solve (95% CI −1.56 to +6.29). The direct comparison of the cache and all-checks finalists included one 13 ms observation on each side in different games. Its interval for cache minus all-checks was −32.60 to +28.95 µs. All observations enter the estimate. [Portable comparisons](development.md#portable)

**The i7-6700K advances `rack-neighbor` and `rack-neighbor-cache`.** In their direct Skylake comparison, `rack-neighbor` averaged 317.90 µs/solve and the cached version averaged 325.26. Adding the cache cost 7.37 µs/solve (95% CI 3.89 to 11.22). Against its matched packed baseline, `rack-neighbor` saved 93.65 µs/solve (78.35 to 109.07). The runner-up advances under the preselected rule. Both targets include `packed` in the held-out comparison. [Skylake comparisons](development.md#skylake) · [Frozen inputs and counters](freeze.json)

The requested `neighbor-cache` combination beat `row-cache` on both targets. Against the stronger `neighbor-bits` constituent it tied on portable (saving 0.76 µs, −3.40 to +5.43) and regressed on Skylake (cost 14.32 µs, 9.96 to 19.13). All five combinations beat packed on both targets. The paired tables include each regression and tie against the individual changes. [All development pairs](development.md)

## Development experiment

Five combinations reuse the measured constructors, row-local check sets and candidate loops. Each row in the paired report compares counters from the same linked library. The round robin covers the five combinations, the strongest earlier counter (`rack`) and `packed`. Five additional pairs cover each individual constituent outside that round robin. [Per-stage means and paired 95% intervals](development.md) · [Per-case observations](pairs) · [Build commands, hashes and qualification counts](manifest.json)

| Combination | Changes to the counter |
| --- | --- |
| `neighbor-cache` | Visit empty columns with a perpendicular neighbor and reuse their check sets within the active row and view. |
| `rack-cache` | Use the actual rack alphabet to stop impossible windows, drop unrestricted gaps and count unrestricted groups in bulk. Cache the square lookups. |
| `rack-neighbor` | Apply the rack policy only to empty columns with a perpendicular neighbor. |
| `rack-neighbor-cache` | Apply that same filter and rack policy, with a row-local cache for the lookups that remain. |
| `rack-all-checks` | Apply the rack policy, then accumulate every retained gap test for each candidate that still needs checking. |

The six pilot games ran first with four pairs per game. The full comparison used ten development games per stage and 40 pairs per game, 20 in each order. Both sides received two warm-ups per observation. One solve starts just before rack-subset generation and ends after restoring the original board view. Rack alphabets, row caches and both directions are inside the clock. Dictionary loading, shared board preparation and validation are outside it. The independent place-and-restore qualifier checked candidate decisions, duplicates, directional totals and the restored board for every measured counter on each target. The release host checked each warm-up against its timed count, and each timed pair matched the qualified across and down counts.

Rank contenders within a target by mean elapsed time, giving each of the six opponents in the round robin equal weight. Keep the two fastest and any additional contender whose direct paired interval against the leader includes zero or favors that contender. Packed accompanies the finalists on each target. The selection rule and executable analysis were committed before timing. Use each direct pair to interpret a saving, since absolute times vary between runs.

### Work done per solve

The full development set supplied 72,530 candidate decisions per counter. The table shows totals across the 30 positions and both views in a separate, untimed trace. With the neighbor filter in place, caching reduced actual calls from 10,289 to 1,715 under rack pruning. Both variants still examined the same 21,624 words and visited the same 27,518 gaps. Those word checks and the traversal of matching windows and rack subsets remain part of each solve. [Development workload by stage](workload.json)

| Counter | Perpendicular-check calls | Words checked individually | Gap tests |
| --- | ---: | ---: | ---: |
| packed | 28,639 | 40,724 | 82,087 |
| row-cache | 5,100 | 40,724 | 82,087 |
| neighbor-bits | 13,076 | 40,724 | 39,974 |
| neighbor-cache | 1,758 | 40,724 | 39,974 |
| all-checks | 28,639 | 40,724 | 124,022 |
| rack | 22,479 | 21,624 | 27,518 |
| rack-cache | 4,645 | 21,624 | 27,518 |
| rack-neighbor | 10,289 | 21,624 | 27,518 |
| rack-neighbor-cache | 1,715 | 21,624 | 27,518 |
| rack-all-checks | 22,479 | 21,624 | 41,680 |

In the linked portable library the candidate loops in `rack`, `rack-cache`, `rack-neighbor` and `rack-neighbor-cache` have equivalent membership checks. Starting at `0x15e9f`, `0x1bd6e`, `0x1c5ef` and `0x1cf5f`, respectively, each compares the mask to the full alphabet, conditionally loads the offset and letter bytes, decrements and zero-extends the letter index, then branches on `bt`. Register allocation, stack offsets and addresses differ. Their construction paths account for different numbers of calls to get a perpendicular-check set. In `rack-all-checks`, the test at `0x1d99e` feeds `setb` and `and`. The trace records 14,162 additional gap tests for that combination. [Linked instruction excerpts](membership.asm)

### Earlier alternatives

- The one-gap shortcut cost 6.90 µs/solve on the pilot. Empty-bit iteration tied packed. Filtering those bits by perpendicular neighbors improved on both. Row-local reuse also beat packed. [One-gap results](../../README.md#one-gap-results) · [Neighbor filter](../../README.md#filter-empty-squares-by-perpendicular-neighbors) · [Row cache](../../README.md#reuse-perpendicular-check-sets-across-overlapping-windows)
- Eager masks and stopping only construction tied packed. Ending the group search at an impossible window saved time. Constrained-only storage improved on that counter. Bulk counting tied per-word counting. Rack pruning improved on the full-alphabet control. Subset checks and multiword-only capacity checks regressed against rack pruning. Checking capacity for all groups tied rack pruning. [Stopping](../../README.md#mask-construction-and-stopping-impossible-windows) · [Constrained storage](../../README.md#constrained-gaps-and-bulk-counting) · [Rack and subset pruning](../../README.md#pruning-with-the-rack-and-matching-rack-subsets)
- Separate gap arrays beat packed, while a 32-bit descriptor tied it. Accumulation improved on early exit. Sorting by selectivity tied early exit. Fixed one- and two-gap loops tied the general loop. Checking a singleton by byte cost time against the singleton bit test. [Layouts](../../README.md#compare-gap-data-layouts) · [Evaluation order](../../README.md#compare-early-exit-accumulation-and-gap-order) · [Specialization](../../README.md#specialize-common-constrained-gap-checks)
- Native bounded SIMD beat scalar accumulation on all-gap windows. Two-pointer batching tied packed and four-pointer batching regressed. The interleave hints produced equivalent batch-loop instructions and tied their width-matched controls. Prefetch helped at width four and tied the plain batch at width two. [SIMD and batching](../window-batches/README.md)
- Clang 18.1.3 at `-O3` improved both rack and packed on portable x86-64 and Skylake. Native branch alignment helped both at `-O2`. LTO, PGO, Skylake scheduling and a size-optimized solver translation unit had counter-dependent effects. We chose portable `-O3 -march=x86-64` and Skylake `-O3 -march=skylake -mbranches-within-32B-boundaries` for the combinations. The target comparison here evaluates those complete configurations. Both use ordinary C++17 builds with no training profile. [Eleven matched compiler configurations and prior training profiles](../window-compiler/README.md)

## Frozen held-out procedure

Use the source revision, host and library hashes, finalist names and profiles in the freeze. The full dictionary has 191,852 words. The input manifest fixes the game, stage and turn for each position. Its 30 development games, 48 reserved PGO games and 96 held-out games are disjoint. These finalist builds used zero training games. For held-out confirmation use every listed held-out game, 32 each from early, middle and late. [Finalist freeze](freeze.json) · [Game and turn membership](../../fixtures/window-game-sets.csv)

Run every pair of finalists and the matched packed baseline within its target, in the recorded order on logical CPU 2 of the i7-6700K. Use the same qualification, solve-only timing, two warm-ups, alternating order and 40 repetitions per game. Average repetitions within each game. Report early, middle and late means for both sides and give the three stages equal weight. A positive paired saving (left minus right) favors the right counter. Sort games by case name within stage and resample paired game differences with replacement inside each stage at its observed size. Use 20,000 bootstrap draws with seed 15 and inclusive 2.5th and 97.5th percentiles. An interval including zero is a tie at measured precision. Compare the leading finalists directly. Keep every valid pair, including long solves. [Interval implementation](../../measurement_support/paired_timings.py)

## Reproduce

From the repository root with the full dictionary, the recorded Clang/LLVM 18 toolchain and the freeze checkout, run the development measurement or analyze the archived pairs. The measurement prints its output directory under `build/window-finalists/development`. Pass that path to the report to analyze a fresh run. Completed pair files can be resumed.

```sh
python3 -u -m bench.window_counters.finalist_trials
python3 -m bench.window_counters.finalist_report
```

To reproduce the untimed trace and inspect linked instructions from the release library listed in the manifest, run the following commands.

```sh
tools=$(make -s window-tools CXX=clang++-18 'CXXFLAGS=-std=c++17 -O3 -march=x86-64 -Wall -Wextra -Iinclude')
python3 -m bench.window_counters.window_profile --build-dir "$tools" --gap-checks \
  --variants packed row-cache neighbor-bits all-checks rack neighbor-cache rack-cache \
  rack-neighbor rack-neighbor-cache rack-all-checks > build/window-finalists-workload.json
cmp bench/results/window-finalists/workload.json build/window-finalists-workload.json
lib=$(python3 -c 'import json; print(json.load(open("bench/results/window-finalists/manifest.json"))["builds"]["portable"]["binary"])')
objdump -d -M intel "$lib" > build/window-finalists-portable.asm
```

The following commands are for issue 14. The runner checks frozen input, host, library and analysis hashes and writes the held-out observations to the directory it prints.

```sh
set -o pipefail
python3 -u -m bench.window_counters.finalist_trials --frozen bench/results/window-finalists/freeze.json | tee build/window-finalists-held-out.log
held_out_results=$(sed -n 's/^results //p' build/window-finalists-held-out.log)
python3 -m bench.window_counters.finalist_report "$held_out_results"
```
