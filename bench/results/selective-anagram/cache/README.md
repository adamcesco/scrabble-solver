# Full-dictionary v3 compatibility

Production and the pinned reader returned **254 across and 102 down** on all
four paths using the sample board and rack `ABFHIKU`. Building from the full
dictionary took 49.77 s and saving took 45.65 s. The build process peaked at
13,169,108 KiB RSS. Both 13,639,098,672-byte v3 files had the same SHA-256.
[Per-process observations](observations.csv)

| Revision | Cache | Operation |
| --- | --- | --- |
| Production | Existing | Load the supplied v3 file and solve. |
| Production | Fresh | Construct the table once from the full dictionary, save it to a new temporary path, then solve using that in-memory table. |
| Production | Fresh | Load the saved file and solve. |
| `823763c` | Fresh | Load the same file and solve. |

Each reader calls `WordPatternTable::load` directly and exits with an error if it
returns an empty table. The runner hashes both cache files before removing its
temporary directory. [Compiler, input and cache identities](provenance.json)

The native worker measures `from_words`, `save`, and `load` separately with a
monotonic clock. GNU time measures the whole process's elapsed seconds and peak
resident memory in KiB, including board preparation and solving.

## Reproduce

Run on Linux from the repository root with Clang, GNU time, and the full `data`
directory available. Supply a new output path on a filesystem with room for the
13.64 GB temporary cache. The lock serializes full-scale qualification runs on
the machine.

```sh
flock /tmp/selective-anagram-full-run.lock python3 -m bench.selective_anagram.cache \
    /path/to/data /tmp/cache-report --production afaba84
```
