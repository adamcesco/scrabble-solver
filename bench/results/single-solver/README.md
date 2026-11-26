# Single-solver refactor qualification

The solver at `cd06293` matched all 302,185 archived candidate decisions on 126 boards. The comparison preserved repeated decisions and checked 252 directional counts. Every solve restored both board views. [Per-board comparisons](qualification/cases.csv) · [Build and reference identities](qualification/provenance.json)

## Prepared solves

On the 96 held-out boards, mean prepared solve time was **258.42 µs at `2dde5e1`** and **253.20 µs at `cd06293`**. Mean time saved was **5.22 µs per solve**, with a paired 95% interval of **3.55–6.78 µs**. The acceptance criterion flags a slowdown when the entire interval is below zero. [Full-precision estimate](timing/summary.json)

Each board contributed 40 paired blocks, 20 in each order. Both workers ran on logical CPU 2 of an Intel i7-6700K with Clang 18.1.3, portable x86-64 and `-O2`. The worker loaded the dictionary and prepared the boards and racks before timing. Every timed solve returned its archived counts. Both processes had zero additional major page faults during the measured blocks. [Raw blocks](timing/blocks.csv.gz) · [Builds, inputs and paging counters](timing/manifest.json)

Reproduce the comparison using `--baseline 2dde5e1 --production cd06293`. [Commands](../../README.md)

## Full dictionary cache

The current solver loaded the existing v3 cache, rebuilt it from the full dictionary, and loaded the rebuilt file in a fresh process. The reader at `823763c` also loaded the rebuilt file. All four paths returned 254 across and 102 down on the sample board with rack `ABFHIKU`. The existing and rebuilt files matched in length and SHA-256. [Observed counts](cache/observations.csv) · [Byte counts, hashes and build identities](cache/provenance.json)
