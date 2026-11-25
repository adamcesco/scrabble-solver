# Held-out window-counter results

**Portable C++17 has two tied leaders, `rack-neighbor-cache` and `rack-neighbor`, built with `-O3 -march=x86-64`.** In their direct comparison they averaged 328.35 and 327.40 µs/solve. Cache minus neighbor was +0.95 µs (95% CI −0.68 to +2.58). The third finalist, `rack-all-checks`, tied `rack-neighbor` in their own pairing (314.58 vs 314.70 µs, difference −0.12, CI −1.65 to +1.36). It lost to `rack-neighbor-cache` by 2.00 µs (318.87 vs 316.88, CI 0.28 to 3.70). Only the two leaders went unbeaten in direct comparisons. Against their matched `packed` controls, the cached and uncached leaders saved 102.89 and 92.32 µs/solve, respectively.

**On the i7-6700K, `rack-neighbor` won with `-O3 -march=skylake -mbranches-within-32B-boundaries`.** It averaged 295.98 µs/solve against 300.76 for `rack-neighbor-cache`, a saving of 4.77 µs (95% CI 3.15 to 6.35). Against the same-build `packed` baseline it averaged 310.16 vs 403.58 µs, saving 93.42 µs (85.19 to 101.88).

`rack-neighbor` uses the rack alphabet to prune windows and only looks up perpendicular checks at empty squares with a perpendicular neighbor. `rack-neighbor-cache` also reuses those lookups within a row. `rack-all-checks` combines rack pruning with evaluating every retained gap test for a candidate. The development report traces the counters to the rack, neighbor, cache and gap-loop experiments and records the branch-alignment decision. [Development comparisons](../README.md#development-experiment) · [Individual experiments](../README.md#earlier-alternatives) · [Compiler and profile trials](../../window-compiler/README.md)

## Games and measurement

Each comparison used the same 96 board-and-rack positions, one turn per game and 32 games per stage. The 30 development games and 48 games reserved for profile training were disjoint from these 96. Both targets used Clang 18.1.3, C++17, the full 191,852-word dictionary and logical CPU 2 of the Intel Core i7-6700K. Neither build used LTO or a PGO profile. The freeze at `ff1db0f` records the solver source at `6c51053`, target flags, executable hashes and input hashes. [Frozen procedure](../freeze.json) · [Game and turn membership](../../../fixtures/window-game-sets.csv)

Before timing, the qualifier checked every counter on every position against an independent place-and-restore validator. It compared candidate decisions with duplicates retained and checked that each solve restored the prepared board. Across the 96 positions each counter saw 118,099 across candidates and 111,556 down candidates in the qualification trace. The validator accepted 21,172 across and 20,322 down. The runner checked both directional counts from each release build on every timed solve against the reference for that position. [Build commands, qualification identities and per-case counts](manifest.json)

Within each pair, both counters solved the same prepared board and rack. Timing started before rack-subset generation and ended after both directions and board restoration, including any counter-specific preparation. Each observation followed two warm-ups per side. Each game contributed 40 paired observations, 20 in each order. That gives 1,280 pairs per stage, 3,840 per comparison and 34,560 across the nine comparisons. [Per-case paired microseconds, run order and directional counts](pairs/)

The tables give mean µs/solve for the left and right counters. Each game contributes its mean over repetitions, and the early, middle and late stage means each receive one third of the weight. The paired saving subtracts right from left within a game, so a positive saving favors the right counter. For each interval the frozen analysis sorts games by case name within stage, resamples 32 paired game differences per stage with replacement, and takes the inclusive 2.5th and 97.5th percentiles of 20,000 draws with seed 15. A paired interval containing zero gives a tie. All observations enter the estimates. [Frozen analysis](../../../window_counters/finalist_report.py) · [Paired interval](../../../measurement_support/paired_timings.py)

Absolute times varied across pairings. For example, portable `packed` averaged 441.35 to 660.41 µs/solve across its three comparisons. Each reported advantage comes from timing the two named counters together. Differences and intervals use the unrounded times.

## Portable C++17

| Left | Right | Early L/R | Middle L/R | Late L/R | Equal-stage L/R | Saving (95% CI) | Outcome |
| --- | --- | ---: | ---: | ---: | ---: | ---: | --- |
| packed | rack-neighbor-cache | 424.90 / 340.32 | 585.82 / 479.27 | 512.30 / 394.78 | 507.67 / 404.79 | +102.89 (+82.81, +123.79) | right faster |
| packed | rack-all-checks | 542.18 / 437.76 | 765.32 / 620.05 | 673.72 / 557.56 | 660.41 / 538.46 | +121.95 (+103.42, +140.58) | right faster |
| packed | rack-neighbor | 382.69 / 301.29 | 505.88 / 400.07 | 435.47 / 345.72 | 441.35 / 349.03 | +92.32 (+83.85, +101.01) | right faster |
| rack-neighbor-cache | rack-all-checks | 283.72 / 282.89 | 359.74 / 363.06 | 307.17 / 310.67 | 316.88 / 318.87 | -2.00 (-3.70, -0.28) | right slower |
| rack-neighbor-cache | rack-neighbor | 287.14 / 287.04 | 376.96 / 374.51 | 320.96 / 320.67 | 328.35 / 327.40 | +0.95 (-0.68, +2.58) | tie |
| rack-all-checks | rack-neighbor | 279.18 / 279.93 | 359.41 / 360.08 | 305.16 / 304.10 | 314.58 / 314.70 | -0.12 (-1.65, +1.36) | tie |

## Intel Core i7-6700K

| Left | Right | Early L/R | Middle L/R | Late L/R | Equal-stage L/R | Saving (95% CI) | Outcome |
| --- | --- | ---: | ---: | ---: | ---: | ---: | --- |
| packed | rack-neighbor | 358.87 / 276.01 | 460.83 / 353.00 | 391.05 / 301.48 | 403.58 / 310.16 | +93.42 (+85.19, +101.88) | right faster |
| packed | rack-neighbor-cache | 355.56 / 284.58 | 457.77 / 361.73 | 389.76 / 305.15 | 401.03 / 317.15 | +83.88 (+73.62, +93.78) | right faster |
| rack-neighbor | rack-neighbor-cache | 263.26 / 270.97 | 334.40 / 339.93 | 290.30 / 291.38 | 295.98 / 300.76 | -4.77 (-6.35, -3.15) | right slower |

## Reproduce

From the results checkout, regenerate both targets' tables from the archived observations.

```sh
python3 -m bench.window_counters.finalist_report bench/results/window-finalists/held-out
```

For a fresh measurement, use a checkout of `ff1db0f` with the full dictionary at `data/dictionaries/sample_dictionary.txt`, the recorded Clang 18.1.3 and GNU ld 2.42 toolchain, and CPU 2 available on the i7-6700K. Run these Bash commands from the repository root. The runner verifies the frozen inputs, protocol and linked binaries before preparing the positions. It resumes completed comparison files in the build directory it prints.

```bash
set -e -o pipefail
mkdir -p build
python3 -u -m bench.window_counters.finalist_trials \
  --frozen bench/results/window-finalists/freeze.json | tee build/window-finalists-held-out.log
held_out_results=$(sed -n 's/^results //p' build/window-finalists-held-out.log)
python3 -m bench.window_counters.finalist_report "$held_out_results"
```
