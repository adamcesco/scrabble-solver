# Prepared rack-neighbor solve time

Reproduce this historical result from checkout `2dde5e1` using the revisions identified below.

**Pass.** Production saved **83.83 µs per prepared solve** on the 96 held-out
games. Its mean was 279.16 µs, down 23.09% from 363.00 µs at `823763c`. The
paired 95% interval for mean time saved was **76.53 to 91.27 µs**. Its lower
bound clears the sole performance gate of zero. [Full-precision estimate](summary.json)

All 3,840 pairs contributed to the estimate. Each game supplied 40 pairs, and
each stage supplied 32 games. Every solve on both revisions returned the
archived across and down counts. The raw rows are in run order and include
both counts and both elapsed times in nanoseconds. [Paired blocks](blocks.csv.gz)

Workers ran on logical CPU 2 of an Intel Core i7-6700K, with the coordinator on
CPU 6. The workers completed 3 preflight sweeps, including two consecutive
sweeps with zero major faults in both workers. Across the measured schedule,
the baseline worker recorded 0 major faults and production recorded 0.
Across the host, 433 pages were swapped in and 7,351 out. [Build, input and paging records](manifest.json)

## Reproduction

From the repository root on Linux with Python 3.10+ and Clang 18.1.3, extract the
frozen reference. Supply the full dictionary and existing v3 cache under the
same `data/` layout used by the solver.

```sh
work=$(mktemp -d)
tar -xzf bench/results/selective-anagram/reference.tar.gz -C "$work"
taskset -c 6 python3 -m bench.selective_anagram.timing measure \
  /path/to/data "$work/reference" "$work/timing" \
  --production afaba84e902985ebb41e91ce4d10bcf1490d5d5e --cpu 2
```

The coordinator extracts the source trees for both revisions, compiles the same
worker against each using portable x86-64 `-O2`, and pins both native workers
to logical CPU 2. Each worker loads the full v3 cache, prepares all 96 boards
and their sorted racks, and stays alive through the final block. The shared
`/tmp/selective-anagram-full-run.lock` serializes full-dictionary qualification.
Both prepared tables must fit in RAM and swap together. The coordinator waits
for two consecutive sweeps across all games with zero major faults in both
workers. The manifest records the counters throughout preflight and across
the timed schedule.

Each repetition shuffles the archived game order with a seeded random
generator. Each game runs 20 blocks in baseline–production order and 20 in
production–baseline order. Each block runs both revisions in its assigned order
three times, recording the final round. The native clock surrounds
`rack_neighbor_both_directions`, including rack-subset generation, both board
views and restoration. The coordinator checks across and down counts on every
response, including warm-ups, after the end timestamp.

To validate the recorded schedule and both directional counts and recompute
the estimate from the raw nanoseconds, run this command after extracting the
reference.

```sh
python3 -m bench.selective_anagram.timing report "$work/reference" \
  bench/results/selective-anagram/timing
```

The report averages each game's 40 paired observations first. It calls the
shared paired-timings estimator with games sorted by identity within early,
middle and late stages. The estimator weights the three stages equally and
resamples paired games within each stage 20,000 times with seed 15. The gate
uses the unrounded lower bound of the paired 95% interval.
