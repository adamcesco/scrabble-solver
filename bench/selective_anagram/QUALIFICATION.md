# Production candidate decisions

From the repository root, run the qualifier with the full dictionary and its
existing v3 cache. Supply a new output directory. The recorded release run used
the production sources at `ca178f4`.

```sh
flock /tmp/selective-anagram-full-run.lock python3 -m bench.selective_anagram.qualification /path/to/data /tmp/decisions --production ca178f4
```

The runner extracts the selected production revision and builds all three public
two-direction solvers with placement tracing and assertions enabled. It replays
the inputs in `bench/results/selective-anagram/reference.tar.gz` after verifying
the dictionary and board hashes. Each solver's traced decisions are compared to
the archived place-and-restore decisions at `823763c` as multisets keyed by
direction, zero-based starting row and column on the normal board, word and
acceptance. Repeated decisions contribute once per occurrence.

For each solve the runner checks the row tiles, occupied masks, care masks and
perpendicular-check grids of both board views against the prepared position.
The output `cases.csv` contains one row per game, solver and direction. Each
row records the size of both multisets, the archived and traced accepted
counts, the count returned by the solver and the result of the restoration
checks. `provenance.json` identifies both revisions, the dictionary and the
reference archive. A failed comparison exits with the game, solver, direction
and a differing decision, count or board field.

Run the comparator regression tests with
`python3 -m bench.selective_anagram.qualification_test`. The default
`build/placement_equivalence_test` fixtures also exercise the board damage
diagnostic.
