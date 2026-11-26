# Solver verification and timing

Run `make test` for the explicit board, rack, candidate-decision and cache fixtures, recorded-game replay and corpus sampling. To inspect a recorded turn, run `python3 -m bench.corpus.replay_game --turn 18`. To export sampled boards with their racks, use `python3 -m bench.corpus.sample_turns --benchmark-dir /tmp/scrabble-boards`.

## Full dictionary

The following commands use Python 3.10+, Linux, Clang and the supplied `data/` directory with its v3 cache. Set `work` to a fresh temporary directory. The qualifier checks the accepted and rejected candidate decisions on all 126 reference boards, both directional counts and restoration of each board. The cache check also builds and reloads a fresh full cache and reads it with the archived reader. Run these memory-intensive commands sequentially.

```sh
work=$(mktemp -d)
python3 -m bench.selective_anagram.qualification data "$work/decisions" --production HEAD
python3 -m bench.selective_anagram.cache data "$work/cache" --production HEAD
```

## Compare prepared solve times

Extract the archived inputs once. Select the baseline and production Git revisions to compile and a logical CPU available to both workers. The baseline defaults to the last revision before the single-solver refactor, `2dde5e1`.

```sh
tar -xzf bench/results/selective-anagram/reference.tar.gz -C "$work"
taskset -c 6 python3 -m bench.selective_anagram.timing measure \
  data "$work/reference" "$work/timing" --baseline 2dde5e1 --production HEAD --cpu 2
python3 -m bench.selective_anagram.timing report "$work/reference" "$work/timing"
```

Each worker prepares all 96 held-out boards before timing. The command checks directional counts on each solve, alternates run order and reports baseline time minus production time with a game-level paired 95% interval. It exits unsuccessfully if the entire interval shows a slowdown. Raw blocks, build identities, paging counters and the summary are written to the output directory.

Archived comparisons include the retired validators, their memory and startup costs, and window-counter experiments. Reproduce them from the recorded revisions. [Historical benchmark guide](HISTORICAL.md) · [Result files](results/)
