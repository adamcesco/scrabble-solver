# Bounded SIMD gap checks and batched candidate loads

Reproduce this historical result from checkout `2dde5e1` using the revisions identified below.

On the 30 development games, bounded SIMD saved **5.87 µs/solve** against the native scalar `all-checks` control (95% CI 3.21 to 8.89) and **19.56 µs/solve** against native `packed` (14.09 to 25.23). On the 24 games outside the pilot, the savings were 6.88 and 20.10 µs/solve, respectively. The direct comparison with `all-checks` isolates the four-gap SIMD path because both counters evaluate every gap.

On the portable build, staging two pointers tied `packed`. Staging four cost 5.78 µs/solve (95% CI 3.68 to 7.95), with only 5.3% of checked words reaching a full batch. Prefetching saved 4.15 µs/solve against the plain four-pointer batch (1.23 to 7.01) and tied `packed` in its direct comparison. At width two, prefetching tied both controls. The interleave hint tied its immediate control at each width, and Clang emitted the same batch-loop instructions.

## Paired solve times

Each row compares two counters in the same release worker on the i7-6700K. The scalar batch trials use Clang 18.1.3 with `-O2 -march=x86-64`. The SIMD trials use `-O2 -march=skylake` for both counters. Time starts before rack-subset generation and ends after counting both views and restoring the board. Each pair uses the same prepared board and rack, two warm-ups per counter and a balanced run order on logical CPU 2. Every timed across and down count matched the qualifier.

The six-game pilot ran four pairs per game. The development runs used 40 pairs per game on all 30 games, 20 in each order. A positive saving favors the challenger. All times are in µs/solve. The 95% intervals resample paired game means within each stage, with equal weight for early, middle and late stages, 20,000 draws and seed 15. The last column uses the 24 development games outside the pilot. [Pilot pairs](pilot.csv.gz) · [Development pairs](development.csv.gz)

| Control | Challenger | Control / challenger | Saving (95% CI) | Saving outside pilot (95% CI) |
| --- | --- | ---: | ---: | ---: |
| `packed` | `batch-2` | 410.54 / 408.23 | +2.31 (-0.75 to +5.45) | +2.25 (-1.26 to +5.74) |
| `batch-2` | `batch-2-interleave` | 398.50 / 401.41 | -2.91 (-6.22 to +0.24) | -2.92 (-7.09 to +1.00) |
| `packed` | `batch-2-interleave` | 403.32 / 407.47 | -4.15 (-7.74 to -0.91) | -5.24 (-9.01 to -1.87) |
| `batch-2` | `batch-2-prefetch` | 401.90 / 403.20 | -1.30 (-3.02 to +0.42) | -0.69 (-2.63 to +1.24) |
| `packed` | `batch-2-prefetch` | 406.02 / 407.12 | -1.11 (-4.23 to +2.22) | -0.40 (-3.97 to +3.42) |
| `packed` | `batch-4` | 397.12 / 402.90 | -5.78 (-7.95 to -3.68) | -6.61 (-8.83 to -4.50) |
| `batch-4` | `batch-4-interleave` | 407.20 / 407.17 | +0.03 (-2.74 to +2.75) | -0.27 (-3.26 to +2.58) |
| `packed` | `batch-4-interleave` | 391.40 / 399.14 | -7.74 (-12.36 to -4.18) | -7.84 (-13.39 to -3.77) |
| `batch-4` | `batch-4-prefetch` | 400.66 / 396.51 | +4.15 (+1.23 to +7.01) | +4.08 (+1.60 to +6.77) |
| `packed` | `batch-4-prefetch` | 403.16 / 403.76 | -0.60 (-2.87 to +1.85) | +0.14 (-2.54 to +2.96) |
| `all-checks` | `bounded-simd` | 381.97 / 376.11 | +5.87 (+3.21 to +8.89) | +6.88 (+3.71 to +10.49) |
| `packed` | `bounded-simd` | 397.97 / 378.42 | +19.56 (+14.09 to +25.23) | +20.10 (+14.52 to +26.26) |

## Which candidates reached each path

The independent place-and-restore solver qualified 72,530 candidate decisions per counter on 30 development games. Packed masks ruled out 31,806 candidates before the word loop. The other 40,724 candidates came from 29,127 groups, including 20,252 singletons. Each counter preserved candidate multiplicity, across and down counts and the board orientation. [Development group-size profile](../window-workload.json)

| Scalar batch path | Two pointers | Four pointers |
| --- | ---: | ---: |
| Groups large enough for a batch | 8,875 | 536 |
| Words in those groups, including remainders | 20,472 | 2,315 |
| Full batches executed | 9,426 | 543 |
| Words in full batches | 18,852 (46.3% of checked words) | 2,172 (5.3%) |
| Words checked in the scalar tail | 21,872 (53.7%) | 38,552 (94.7%) |
| Groups with a scalar tail | 21,872 | 28,733 |
| Groups with both a full batch and a tail | 1,620 | 142 |
| In-range prefetches in the prefetch trial | 2,171 | 149 |

These counts apply to the default, interleave and prefetch trials of each width. A group of size `s` that enters the word loop runs `s / width` whole batches (integer division) and `s % width` scalar checks. Each prefetch trial requests the first word after the current batch when that word exists, including a word in the tail. These frequencies come from the observed individual checks in each group-size bucket.

SIMD works across four gap masks of **one** candidate. It reached 13,111 words (32.2% of individually checked words), including candidates in singleton groups. Of these, 9,559 ended after the four-lane check and 3,552 needed one to three additional scalar checks. The remaining 27,613 words had one to three gaps and used the scalar path throughout. [Untimed gap counts, group sizes and qualification totals](profile.json)

| Stage | Individually checked words | In two-pointer batches | In four-pointer batches | SIMD words | SIMD words with a scalar tail |
| --- | ---: | ---: | ---: | ---: | ---: |
| Early | 12,228 | 5,466 | 736 | 4,153 | 1,071 |
| Middle | 19,512 | 9,990 | 1,212 | 6,955 | 2,059 |
| Late | 8,984 | 3,396 | 224 | 2,003 | 422 |

## Linked workers and allocation bounds

All scalar batch variants use packed mask construction, skip unrestricted masks and stop at the first failed gap. Clang 18.1.3 emitted two 32-bit word-ID loads and two 64-bit pointer loads at `0x827f`–`0x828c` in the portable two-pointer visitor. The four-pointer visitor stages four of each at `0x8b14`–`0x8b30`. Each then reads single bytes at the gap offsets. The batch loops advance by two and four respectively. The remainders advance one ID at a time. In the prefetch trials an unsigned range check at `0xa60a` or `0xaec0` skips the future ID load, pointer load and `prefetcht0` together.

The interleave trials apply Clang's `interleave_count(2)` to the full-batch loop. Each hinted group visitor has the same instructions as its default counterpart, including branch targets relative to the loop. Only a relative call displacement in mask preparation differs. The visitors occupy 750 bytes at width two and 970 bytes at width four.

The Skylake SIMD visitor reads four bytes at four gap offsets using separate `movsx BYTE PTR` instructions at `0xdacb`–`0xdada`. It inserts the characters into registers, shifts four 32-bit masks with `vpsllvd` and extracts their membership bits with `vmovmskps`. Its 16-byte descriptor load reads the local gap array. For five, six and seven gaps, the linked tail reads one further word byte at each of `0xdb1c`, `0xdb46` and `0xdb70`, guarded by the gap count. Every matched word has the window's length, and all gap offsets fall within that length.

The immediate SIMD control, `all-checks`, builds the same masks and evaluates every gap. Its linked loop uses a one-byte character load and scalar `bt` at `0xd2a8` and `0xd2b4` for every supported rack size. Clang also emitted a vector loop guarded by a minimum of 33 gaps at `0xce31`. A rack supplies at most seven. Both counters recorded 124,022 gap checks and the same first-failure histogram in each stage.

The AVX2 intrinsics and registry entry compile only into an AVX2-enabled experiment build. The portable qualifier and release worker ran with QEMU's baseline x86-64 CPU features, including two passing across placements and one down on the crossings fixture. The native AddressSanitizer and UndefinedBehaviorSanitizer qualification exercised word lengths two through seven, with the failing crossing moved through every character, including the final one. The partial-group fixture exercised five words in one group and a singleton in the next.

## Reproduce the comparison

The commands below use the checked-in case identities, the project dictionary and its matching cache. The native worker requires a CPU that supports the instructions selected by `-march=skylake`. Set `cpu` to an available logical CPU. The recorded runs used CPU 2 on an Intel Core i7-6700K.

```sh
set -eu
cpu=2
out=build/batch-comparison
mkdir -p "$out"
python3 -m bench.window_counters.window_games --export development "$out/development"
python3 -m bench.corpus.sample_turns --verify bench/fixtures/pilot/corpus.json --window-dir "$out/pilot"
portable=$(make -s window-tools CXX=clang++ CXXFLAGS='-std=c++17 -O2 -march=x86-64 -Wall -Wextra -Iinclude')
native=$(make -s window-tools CXX=clang++ CXXFLAGS='-std=c++17 -O2 -march=skylake -Wall -Wextra -Iinclude')

compare() {
    taskset -c "$cpu" python3 -m bench.window_counters.window_benchmark --build-dir "$1" \
        --dictionary data/dictionaries/sample_dictionary.txt --variants "$2" "$3" \
        --cases "$out/$dataset/cases.csv" --repetitions "$repetitions" > "$out/$dataset-$2-$3.csv"
}
for dataset in pilot development; do
    repetitions=40
    if [ "$dataset" = pilot ]; then repetitions=4; fi
    for width in 2 4; do
        compare "$portable" packed "batch-$width"
        for hint in interleave prefetch; do
            compare "$portable" "batch-$width" "batch-$width-$hint"
            compare "$portable" packed "batch-$width-$hint"
        done
    done
    compare "$native" all-checks bounded-simd
    compare "$native" packed bounded-simd
done

python3 -m bench.window_counters.window_profile --build-dir "$native" --gap-checks \
    --variants packed all-checks bounded-simd > "$out/profile.json"
cmp "$out/profile.json" bench/results/window-batches/profile.json
objdump -d -C -M intel "$portable/window_worker" > "$out/portable.asm"
objdump -d -C -M intel "$native/window_worker" > "$out/native.asm"
python3 -m bench.results.window-batches.summarize
```

The last command reproduces the estimates from the archived development pairs. Pass `"$out"/development-*.csv` to summarize a fresh run. To inspect allocation bounds under instrumentation, build and run the native qualifier separately.

```sh
flags='-std=c++17 -O1 -g -march=skylake -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Iinclude'
sanitized=$(make -s window-tools CXX=clang++ CXXFLAGS="$flags")
UBSAN_OPTIONS=halt_on_error=1 "$sanitized/placement_equivalence_test"
```

To repeat the portable check with QEMU 8.2, run the qualifier and the linked worker under the same CPU model.

```sh
model=qemu64,-sse3,-ssse3,-sse4.1,-sse4.2,-popcnt,-lahf-lm,-cx16
fixture=bench/fixtures/paired
qemu-x86_64 -cpu "$model" "$portable/placement_equivalence_test" "$fixture/dictionary.txt" "$out/fixture-cache" batch-2 batch-4 </dev/null
printf '%s\tRT\t0\n' "$fixture/crossings.csv" | qemu-x86_64 -cpu "$model" "$portable/window_worker" \
    batch-2 batch-4 "$fixture/dictionary.txt" "$out/fixture-cache"
```
