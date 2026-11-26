# Qualify the solver against frozen candidate decisions

From the repository root, supply the full dictionary and v3 cache under the usual data layout and a new output path.

```sh
python3 -m bench.selective_anagram.qualification /path/to/data /tmp/decisions --production HEAD
```

The runner builds the selected revision with placement tracing, verifies each archived input and checks both board views after every solve. For each board and direction it compares the full multiset of accepted and rejected decisions, including multiplicities, with the historical reference. It also checks the returned count against the accepted decisions.

The report contains a row per board and direction in `cases.csv`. The provenance records the solver and reference revisions, build inputs, dictionary and archive hashes. [Reference source and regeneration](README.md)
