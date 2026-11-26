# Frozen candidate decisions

The [reference archive](../results/selective-anagram/reference.tar.gz) contains 126 board/rack inputs with directional candidate decisions, including repeated acceptances and rejections. The provenance identifies the place-and-restore implementation at `823763c` that supplied those decisions. Each CSV records direction, zero-based starting row and column on the normal board, word, and acceptance.

To regenerate the archive, run the original capture driver in a checkout at `2dde5e1`. It extracts `823763c` itself and checks dictionary, board and manifest identities.

```sh
git worktree add --detach ../scrabble-reference 2dde5e1
cd ../scrabble-reference
python3 -m bench.selective_anagram.reference /path/to/data /tmp/new-reference
```

Use the archived decisions to qualify the current solver. [Qualification](QUALIFICATION.md)
