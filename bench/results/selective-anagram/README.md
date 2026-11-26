# Selective anagram lookup release qualification

Reproduce this historical result from checkout `2dde5e1` using the revisions identified below.

The production rack-neighbor solver saved **83.83 µs per prepared solve** against `823763c` on the 96 held-out games. The paired 95% interval was **76.53–91.27 µs**, passing the release criterion. The measured production revision was `afaba84`. [Raw paired blocks](timing/blocks.csv.gz) · [Method and reproduction](timing/README.md)

| Prepared solve measurement | Result |
| --- | ---: |
| Pre-change mean | 363.00 µs |
| Production mean | 279.16 µs |
| Reduction | 23.09% |
| Matched blocks | 3,840 |

The timing workers used the same compiler, portable optimization flags, CPU and board inputs. Each game contributed 20 pairs in each order, with two warm-up rounds per measured round. Both workers finished every measured solve with the expected across and down counts. [Build identities and paging observations](timing/manifest.json)

## Candidate decisions

Rack-neighbor, packed and place-and-restore each matched every candidate decision from the pinned place-and-restore reference. Each comparison retained repeated decisions and checked the direction, starting square, word and acceptance. The returned directional counts matched, and all 378 solves restored both board views. [Per-case comparisons](qualification/cases.csv) · [Reproduce the comparison](../../selective_anagram/QUALIFICATION.md)

| Cohort | Games | Decisions per solver | Accepted per solver |
| --- | ---: | ---: | ---: |
| Development | 30 | 72,530 | 12,939 |
| Held-out | 96 | 229,655 | 41,494 |

The compressed reference contains the board and full decision trace for each game, the rack, input hashes and counts. Extract it with `tar -xzf bench/results/selective-anagram/reference.tar.gz -C /path/to/output` from the repository root, or generate the traces again from the pinned revision. [Reference archive](reference.tar.gz) · [Regenerate the reference](../../selective_anagram/README.md)

## Full-dictionary v3 compatibility

With rack `ABFHIKU`, the sample board returned **254 across and 102 down** on all four paths. Production loaded the existing cache, built and saved a fresh table, and loaded the fresh file in a new process. A separate `823763c` process loaded the same fresh file. Both cache files were 13,639,098,672 bytes and had the same SHA-256 digest.

Construction took 49.77 seconds and saving took 45.65 seconds. The build process peaked at 13,169,108 KiB RSS. [Observations and reproduction](cache/README.md)
