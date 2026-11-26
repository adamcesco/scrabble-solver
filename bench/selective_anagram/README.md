# Selective anagram lookup qualification

## Independent candidate decisions

From a checkout containing commit `823763c`, pass the data directory with
`dictionaries/sample_dictionary.txt` and its existing v3
`caches/pattern_and_rack_anagram_to_words.cache`, followed by a new output path.

```sh
python3 -m bench.selective_anagram.reference /path/to/data /tmp/reference
```

The runner extracts the pinned revision and uses its compiler settings, corpus
exporter and placement-comparison harness. It checks the dictionary, each board
SHA-256, rack, stage and case identity against the archived finalist manifests.
The place-and-restore trace supplies the decisions. The harness checks the
directional counts and restoration of both board views. The runner reads back the
decision files and verifies the accepted and total candidate counts in both
directions for each game.

In the output, `cases.csv` identifies the position, rack and counts for each
cohort-relative board path. For example, `inputs/development/board-1.csv` holds
the first board, and `decisions/development/board-1.csv` holds its trace.
Each decision gives the direction, zero-based row and column of the starting
square on the normal board, word, and acceptance (1 or 0). Repeated decisions
occupy separate rows. Compare each file as a multiset. The JSON provenance
records the full solver revision and dictionary SHA-256.
