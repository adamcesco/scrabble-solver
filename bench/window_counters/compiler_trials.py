"""Time matched compiler builds sequentially on CPU 2. Rerun to resume completed pairs."""
import csv
import fcntl
import gzip
import json
import math
import os
import shlex
import subprocess
from contextlib import ExitStack, contextmanager
from pathlib import Path

from bench.corpus.sample_turns import PILOT, case_name, write_benchmark
from bench.measurement_support.benchmark_inputs import Position, qualify_positions, read_manifest, select_cache, sha256
from bench.window_counters.tuning_builds import CONFIGS, EDGES, NATIVE, PORTABLE, WORK, build, identity, write_json
from bench.window_counters.window_benchmark import WindowCase, paired_order
from bench.window_counters.window_games import reserved_cases

DICTIONARY = Path("data/dictionaries/sample_dictionary.txt")
VARIANTS = ("packed", "rack")
CPU = 2


def inputs():
    digest = sha256(DICTIONARY)
    cases, manifest = {}, {}
    for dataset in ("development", "pgo"):
        games = reserved_cases(dataset)
        if any(game["dictionary_sha256"] != digest for game in games):
            raise ValueError("dictionary differs from the frozen corpus")
        directory = WORK / dataset
        write_benchmark(directory, games, paired=False)
        path = directory / "cases.csv"
        cases[dataset] = []
        for row in read_manifest(path):
            position = Position.from_row(path, row)
            cases[dataset].append(WindowCase(row["case"], row["stage"], position))
            manifest[row["case"]] = dict(dataset=dataset, stage=row["stage"], rack=position.rack,
                                         board_sha256=sha256(position.board))
    names = {case_name(g) for g in json.loads((PILOT / "corpus.json").read_text())["cases"]}
    cases["pilot"] = [c for c in cases["development"] if c.name in names]
    return cases, manifest


def qualification(cases, manifest, host):
    directory = Path(subprocess.check_output(
        ["make", "-s", "--no-print-directory", "window-tools", "CXX=clang++-18",
         "CXXFLAGS=-std=c++17 -O2 -march=x86-64 -Wall -Wextra -Iinclude"], text=True).strip())
    cache = select_cache(host, DICTIONARY, WORK / "patterns")
    key = identity([manifest, sha256(DICTIONARY), *VARIANTS, sha256(directory / "placement_equivalence_test"),
                    sha256(directory / "perpendicular_checks_test")])
    saved = WORK / f"qualified-{key[:20]}.json"
    if not saved.exists():
        print("qualify development and training positions", flush=True)
        positions = [c.position for c in cases]
        counts = qualify_positions(DICTIONARY, cache, positions, VARIANTS, directory)
        write_json(saved, {c.name: counts[c.position] for c in cases})
    return cache.path, json.loads(saved.read_text()), key


@contextmanager
def session(host, cache, env=None):
    proc = subprocess.Popen([host, DICTIONARY, cache], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                            text=True, env=env)
    try:
        yield proc
        proc.stdin.close()
        if proc.wait():
            raise RuntimeError("compiler comparison worker failed")
    finally:
        if proc.poll() is None:
            proc.kill()
            proc.wait()
        proc.stdin.close()
        proc.stdout.close()


def observations(proc, binaries, variants, cases, reps, counts):
    left, right = binaries
    a, b = variants
    proc.stdin.write(f"@pair\t{left}\t{a}\t{right}\t{b}\n")
    proc.stdin.flush()
    for path, variant in zip(binaries, variants, strict=True):
        if proc.stdout.readline().rstrip("\n") != f"{path}\t{variant}":
            raise ValueError(f"worker did not resolve {variant} to {path}")
    for rep, case, first in paired_order(cases, reps):
        position = case.position
        proc.stdin.write(f"{position.board}\t{position.rack}\t{first}\n")
        proc.stdin.flush()
        times = [0., 0.]
        expected = counts[case.name]
        for index in (first, 1 - first):
            name, across, down, us = proc.stdout.readline().strip().split(",")
            if name != variants[index] or (int(across), int(down)) != (expected["across"], expected["down"]):
                raise ValueError(f"{case.name}: {name} release counts differ from qualification")
            times[index] = float(us)
            if not math.isfinite(times[index]) or times[index] <= 0:
                raise ValueError(f"{case.name}: invalid solve duration {us}")
        yield dict(case=case.name, stage=case.stage, repetition=rep, first=first,
                   across=expected["across"], down=expected["down"],
                   left_us=times[0], right_us=times[1])


def train(target, instrumented, host, cache, cases, counts, qualification_id):
    key = identity([instrumented["sha256"], host["sha256"], qualification_id, [c.name for c in cases], VARIANTS])
    directory = WORK / "profiles" / key[:20]
    directory.mkdir(parents=True, exist_ok=True)
    profile = directory / "training.profdata"
    stamp = directory / "training.json"
    if stamp.exists() and profile.exists():
        record = json.loads(stamp.read_text())
        if record["profile_sha256"] == sha256(profile):
            return profile, record
    raw = directory / "training.profraw"
    raw.unlink(missing_ok=True)  # LLVM can merge into an existing raw profile.
    print(f"train {target} on {len(cases)} games", flush=True)
    env = dict(os.environ, LLVM_PROFILE_FILE=str(raw))
    with session(host["binary"], cache, env) as proc:
        for _ in observations(proc, [instrumented["binary"]] * 2, VARIANTS, cases, 1, counts):
            pass
    command = ["llvm-profdata-18", "merge", "-o", str(profile), str(raw)]
    subprocess.run(command, check=True)
    # Read the two typed solve entry counts from the merged profile itself.
    show = ["llvm-profdata-18", "show", "--counts", "--function=:_ZN12_GLOBAL__N_113measure_solve", str(profile)]
    details = subprocess.check_output(show, text=True)
    expected = 3 * len(cases)
    entries = [int(line.split(":")[1]) for line in details.splitlines() if "Function count:" in line]
    if entries != [expected, expected]:
        raise ValueError(f"{target}: profile recorded {entries}, expected {expected} per counter")
    record = dict(profile_counters=details, instrumented=instrumented["sha256"], raw_sha256=sha256(raw), profile_sha256=sha256(profile),
                  games=[c.name for c in cases], solves_per_counter=expected,
                  counters=VARIANTS, merge=shlex.join(command), raw_file=str(raw))
    write_json(stamp, record)
    return profile, record


def comparisons(builds):
    for name in builds:
        yield (name, "packed"), (name, "rack")
    for control, challenger in EDGES:
        for variant in VARIANTS:
            yield (control, variant), (challenger, variant)


def run():
    os.sched_setaffinity(0, {CPU})
    host = build("host", PORTABLE)
    cases, manifest = inputs()
    cache, counts, qualification_id = qualification(cases["development"] + cases["pgo"], manifest, host["binary"])
    builds = {name: build(name, flags) for name, flags in CONFIGS.items()}
    training = {}
    for target, flags in (("portable", PORTABLE), ("native", NATIVE)):
        trainer = build("train-" + target, (*flags, "-fprofile-instr-generate"))
        profile, record = train(target, trainer, host, cache, cases["pgo"], counts, qualification_id)
        builds[target + "-pgo"] = build(target + "-pgo", flags, profile=profile)
        training[target] = dict(record, build=trainer)
    cpuinfo = dict(line.split(": ", 1) for line in Path("/proc/cpuinfo").read_text().split("\n\n")[0]
                   .replace("\t", "").splitlines() if ": " in line)
    metadata = dict(host=host, builds=builds, training=training, inputs=manifest, counts=counts,
                    dictionary_sha256=sha256(DICTIONARY), qualification_id=qualification_id,
                    schedule={"pilot": 4, "development": 40}, cpu=cpuinfo["model name"], microcode=cpuinfo.get("microcode"), affinity=CPU,
                    runner_sha256=sha256(Path(__file__)), kernel=os.uname().release)
    # A changed binary, profile, input or schedule starts a separate run.
    output = WORK / "runs" / identity(metadata)[:20]
    output.mkdir(parents=True, exist_ok=True)
    write_json(output / "manifest.json", metadata)
    print(f"results {output}", flush=True)
    with ExitStack() as stack:
        proc = None
        for split, reps in metadata["schedule"].items():
            for left, right in comparisons(builds):
                a, b = left
                c, d = right
                filename = output / f"{split}_{a}.{b}_{c}.{d}.csv.gz"
                if filename.exists():
                    continue
                if proc is None:
                    proc = stack.enter_context(session(host["binary"], cache))
                rows = observations(proc, (builds[a]["binary"], builds[c]["binary"]),
                                    (b, d), cases[split], reps, counts)
                tmp = filename.with_suffix(".partial")
                with gzip.open(tmp, "wt", newline="") as stream:
                    writer = csv.DictWriter(stream, fieldnames=("case", "stage", "repetition", "first", "across",
                                                                "down", "left_us", "right_us"), lineterminator="\n")
                    writer.writeheader()
                    writer.writerows(rows)
                tmp.replace(filename)
                print(f"saved {filename.name}", flush=True)


if __name__ == "__main__":
    WORK.mkdir(parents=True, exist_ok=True)
    with (WORK / "run.lock").open("w") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        run()
