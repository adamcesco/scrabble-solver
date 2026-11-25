# Clang tuning for the packed and rack counters

On the 30 development games, **`rack` beat `packed` in all eleven matched builds**. With portable `-O2` the saving was 105.54 µs/solve (95% CI 81.10 to 133.52). With Skylake `-O2` it was 76.73 µs/solve (62.19 to 91.15). The portable pair includes two packed observations above 9 ms. All observations enter the mean. Even the smallest matched implementation saving, 72.72 µs with portable PGO, exceeded the largest measured flag saving for either counter.

Holding the counter fixed, **`-O3` improved both implementations on both targets**. Native branch alignment saved 18.95 µs for `packed` and 12.08 µs for `rack`. Switching from Skylake scheduling on baseline x86-64 to Skylake instruction selection tied for both. Tuning alone helped `packed` and slowed `rack`. The `-Os` solver translation unit saved 5.89 µs for native `rack` and tied for native `packed`.

PGO improved portable `packed` by 16.98 µs and slowed native `packed` by 7.23 µs. Portable `rack` tied its control. Native `rack` saved 4.92 µs on all 30 games, with a CI of 0.15 to 9.99, and tied on the 24 outside the pilot. Full LTO helped portable `packed` and native `rack`. Compare settings using the paired rows below. The separately recorded absolute times vary across comparisons. Interactions among settings and held-out selection belong to the subsequent finalist experiments.

## Reproduce or resume

Run from the repository root with Clang and LLVM 18, GNU ld with the LLVMgold plugin, and the full project dictionary. The recorded host was an Intel Core i7-6700K under WSL2. Ubuntu Clang 18.1.3 targeted x86_64-pc-linux-gnu. The runner pins itself and its children to logical CPU 2. The Skylake libraries require a compatible execution CPU.

```sh
python3 -u -m bench.window_counters.compiler_trials
python3 -m bench.window_counters.tuning_report
```

The first command prints its results directory under `build/window-compiler/runs/`. Pass that directory to the second command to analyze a fresh run. Its default is the archived run here. Each comparison writes a temporary gzip CSV, checks both counters on every pair, then renames the completed file. Rerunning the first command retrieves matching builds and profiles and skips completed CSVs. A lock serializes concurrent invocations. The build records include every compile and link command, source hashes, compiler, linker, target flags, profile SHA-256, linked binary SHA-256 and linked `.text` size in bytes. [Run manifest](manifest.json) · [Raw paired times](pairs)

A second invocation completed with all 62 checkpoint modification times unchanged. In a separate loader check, both selected solve addresses resolved successively to portable, native, portable-PGO, native-PGO and portable again. The verified binary SHA-256 prefixes were `7a92b59173da`, `28275ab9169b`, `2de215f472a2`, `52d98d5ccd4a` and `7a92b59173da`. The two profile-use binaries used profiles `1b718286370a` and `a055f92ae724`, respectively.

## What was measured

`packed` is the production counter. `rack` uses the actual rack alphabet to rule out impossible windows, drops unrestricted gaps, and accepts an unrestricted matching group in bulk. Ticket 06 measured a saving for this policy, and it runs on both target ISAs. Both counters use the existing typed solve driver. The clock covers rack-subset generation, counter setup, both board views, and restoration of the starting view.

Each configuration builds a linked shared library with these two counters registered. Internal solver symbols have hidden visibility. A single fixed host loads the full dictionary and pattern table, prepares the same board for both sides, and dispatches to each selected solve driver before its clock starts. The host verifies the actual library owning each selected solve address using `dladdr` when switching comparisons. This permits direct flag comparisons on one prepared position without keeping two copies of the full table in memory.

The independent place-and-restore qualifier checked 72,530 candidate decisions per counter on 30 development games and 117,348 on 48 training games, including multiplicities, directional totals and restored board views. The release host checks that both warm-ups and the measured solve agree. The runner checks each recorded directional count against the qualifier. In the raw CSV, `first=0` means the left side ran first and `first=1` means the right side ran first. Each of the 11 builds has a direct implementation comparison. Each of the ten changes in build settings has separate comparisons holding `packed` and `rack` fixed.

The pilot used four repetitions on six games. Development used 40 repetitions on 30 games, ten per stage. Which side went first alternated within stage and across repetitions. Each round ran both sides in that order, twice to warm up and once for the recorded times. The analysis averages repetitions within game, weights the three stage means equally, and resamples paired games within stage for 20,000 deterministic bootstrap draws. The last column repeats the analysis on the 24 development games outside the pilot. A positive saving favors the right side. Intervals spanning zero are ties at this precision. Held-out games are reserved for the later finalist comparison. [Frozen game sets](../../fixtures/window-game-sets.csv)

## Training profiles

Each target trained one shared profile on the same 48 reserved positions, sixteen per stage. The instrumented library ran `packed` and `rack` three times each per position, for 144 solves per counter. The merged profile's two typed solve entry counts independently confirm that exposure. Both algorithms then ran from the same profile-use library for that target. The recorded timing matrix contains only ordinary and profile-use builds. ELF section inspection found LLVM profile counters in both trainers and zero profile counter sections in all eleven timed libraries. The manifest retains the raw and merged profile hashes, both training builds, the merge commands and LLVM's entry counts. [Portable profile](profiles/portable.profdata) · [Skylake profile](profiles/native.profdata)

## Linked code

The disassembly is from the final libraries that supplied the solve addresses. The compiler remarks cover the solver translation unit. LTO also optimizes at link time. Sizes below refer to the linked library's `.text` section, which includes cold code. In particular, full LTO discards library code unreachable from the exported registry lookup. [Linked instructions](linked-assembly.txt.gz) · [Clang optimization remarks](optimization-remarks.txt.gz)

- Skylake scheduling with the baseline x86-64 ISA grew the packed and rack group visitors from 422 and 449 bytes to 723 and 678. The generic rack visitor has one scalar `bt` candidate check. The tuned visitor has three static copies. Native instruction selection keeps those checks and uses BMI2 `shlx` in mask setup at `0x7c60`, where the tuned build uses `shl esi,cl` at `0x7a05`.
- Native `-O2` leaves an out-of-line rack group visitor. Clang reports an inline cost of 425 against a threshold of 325 at the binary-probe and sorted-merge call sites. Compiling just the solver translation unit at `-Os` shrinks the rack visitor from 678 to 324 bytes and the packed visitor from 739 to 313. Both compact visitors call the same out-of-line word checker, at `0x7bf5` and `0x7dc4`. The native `-O2` visitors inline it. The other eight linked objects match across the size trial byte for byte.
- With Clang's default alignment, the native rack visitor has three conditional jumps that cross a 32-byte boundary, at `0x7bfd`, `0x7c7f` and `0x7cdf`. Enabling `-mbranches-within-32B-boundaries` removes all three crossings and grows this visitor to 710 bytes. Both versions retain the same three scalar `bt` membership tests. The guest reports family 6, model 94, stepping 3 and microcode `0xffffffff`. The run did not collect hardware front-end events, so the timing measures the effect of the flag on these libraries, without isolating a microarchitectural cause.
- Native `-O3` and native PGO inline the rack group visitor into the per-direction traversal. Its body grows from 1,441 bytes at `-O2` to 2,855 and 2,551 bytes respectively. In both, the two remaining direct calls in that traversal fetch the perpendicular-check set. LTO keeps the visitor out of line and inlines both calls to `Board::swap_views` into the solve driver. The rack membership sequence is equivalent across tuned, native and native LTO at `0x7af5`, `0x7d55` and `0x2f35`: two byte loads, decrement, zero extension, bit test and conditional jump. Aligned code at `0x7f49` uses the same instructions with one padding byte before the jump. Changes elsewhere in the solve can still affect its elapsed time.

| Build | Target ISA and tuning | Change from target's default `-O2` | Linked .text (bytes) |
| --- | --- | --- | ---: |
| portable | x86-64, generic | Reference | 43,141 |
| portable-o3 | x86-64, generic | `-O3` | 50,213 |
| tuned | x86-64, Skylake | `-mtune=skylake` | 44,629 |
| native | Skylake | `-march=skylake` | 45,141 |
| native-o3 | Skylake | `-O3` | 52,325 |
| portable-lto | x86-64, generic | Full LTO | 8,094 |
| native-lto | Skylake | Full LTO | 10,286 |
| aligned | Skylake | Align branches within 32-byte regions | 46,389 |
| solver-os | Skylake | Solver translation unit at `-Os` | 43,445 |
| portable-pgo | x86-64, generic | Portable profile use | 40,972 |
| native-pgo | Skylake | Skylake profile use | 41,068 |

## Paired development results

| Left build · counter | Right build · counter | Left / right µs | Saving µs (95% CI) | Saving outside pilot (95% CI) |
| --- | --- | ---: | ---: | ---: |
| aligned.packed | aligned.rack | 423.40 / 346.74 | +76.66 (+61.89, +91.52) | +82.09 (+64.63, +99.38) |
| native.packed | native.rack | 404.54 / 327.81 | +76.73 (+62.19, +91.15) | +79.17 (+62.01, +96.02) |
| native-lto.packed | native-lto.rack | 425.32 / 336.66 | +88.66 (+72.11, +105.26) | +94.00 (+73.88, +113.37) |
| native-o3.packed | native-o3.rack | 409.87 / 326.57 | +83.31 (+67.70, +98.42) | +86.16 (+67.65, +103.83) |
| native-pgo.packed | native-pgo.rack | 430.73 / 345.87 | +84.85 (+69.45, +99.94) | +88.04 (+69.44, +105.85) |
| portable.packed | portable.rack | 458.75 / 353.21 | +105.54 (+81.10, +133.52) | +111.08 (+81.98, +144.34) |
| portable-lto.packed | portable-lto.rack | 440.16 / 350.19 | +89.97 (+73.17, +106.13) | +93.77 (+73.79, +112.63) |
| portable-o3.packed | portable-o3.rack | 409.33 / 332.30 | +77.03 (+60.43, +93.00) | +79.96 (+59.69, +98.74) |
| portable-pgo.packed | portable-pgo.rack | 419.80 / 347.08 | +72.72 (+58.94, +86.34) | +75.38 (+59.35, +90.70) |
| solver-os.packed | solver-os.rack | 425.97 / 340.86 | +85.11 (+69.57, +100.01) | +86.45 (+67.98, +104.02) |
| tuned.packed | tuned.rack | 429.60 / 351.45 | +78.16 (+62.22, +94.00) | +83.13 (+64.35, +101.30) |
| portable.packed | portable-o3.packed | 434.09 / 419.81 | +14.28 (+9.43, +19.29) | +14.75 (+9.94, +19.95) |
| portable.rack | portable-o3.rack | 342.51 / 337.95 | +4.56 (+1.25, +7.90) | +3.70 (+0.62, +6.86) |
| portable.packed | tuned.packed | 430.81 / 423.95 | +6.86 (+3.38, +10.52) | +6.39 (+2.48, +10.48) |
| portable.rack | tuned.rack | 335.82 / 344.86 | -9.04 (-14.37, -4.45) | -8.23 (-14.04, -3.17) |
| tuned.packed | native.packed | 424.87 / 423.74 | +1.13 (-2.23, +4.45) | +1.76 (-1.75, +5.41) |
| tuned.rack | native.rack | 336.02 / 334.65 | +1.38 (-2.09, +5.15) | +1.70 (-2.37, +6.06) |
| native.packed | native-o3.packed | 427.19 / 415.87 | +11.32 (+7.14, +15.32) | +11.66 (+6.42, +16.52) |
| native.rack | native-o3.rack | 342.77 / 332.94 | +9.83 (+5.78, +13.73) | +8.95 (+4.14, +13.51) |
| portable.packed | portable-lto.packed | 420.88 / 415.67 | +5.21 (+1.55, +8.96) | +6.65 (+2.54, +10.85) |
| portable.rack | portable-lto.rack | 346.86 / 343.63 | +3.23 (-1.85, +7.65) | +3.94 (-2.22, +9.30) |
| native.packed | native-lto.packed | 424.27 / 425.48 | -1.21 (-4.42, +2.01) | -1.71 (-5.21, +1.82) |
| native.rack | native-lto.rack | 344.20 / 335.70 | +8.51 (+3.60, +13.74) | +8.07 (+2.89, +13.40) |
| portable.packed | portable-pgo.packed | 422.40 / 405.43 | +16.98 (+12.36, +21.54) | +18.20 (+12.80, +23.49) |
| portable.rack | portable-pgo.rack | 335.99 / 334.06 | +1.93 (-2.05, +5.80) | +1.85 (-2.42, +6.28) |
| native.packed | native-pgo.packed | 417.45 / 424.67 | -7.23 (-11.94, -2.76) | -7.50 (-12.80, -2.36) |
| native.rack | native-pgo.rack | 335.44 / 330.53 | +4.92 (+0.15, +9.99) | +3.51 (-1.18, +8.73) |
| native.packed | aligned.packed | 437.74 / 418.79 | +18.95 (+9.12, +29.90) | +20.68 (+10.01, +32.46) |
| native.rack | aligned.rack | 347.64 / 335.57 | +12.08 (+8.49, +16.11) | +11.78 (+7.73, +16.36) |
| native.packed | solver-os.packed | 419.28 / 420.43 | -1.15 (-5.79, +3.42) | -1.52 (-6.35, +3.08) |
| native.rack | solver-os.rack | 348.21 / 342.32 | +5.89 (+2.46, +9.58) | +5.90 (+2.09, +9.88) |
