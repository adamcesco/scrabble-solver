"""Build a counter library or its host with the exact recorded Clang configuration."""
import gzip
import hashlib
import json
import shlex
import shutil
import subprocess
from pathlib import Path

from bench.measurement_support.benchmark_inputs import sha256

WORK = Path("build/window-compiler")
HERE = Path("bench/window_counters")
SOLVER = Path("src/rack_oriented_solver.cpp")
SOURCES = sorted(set(Path("src").glob("*.cpp")) - {Path("src/main.cpp")})
PORTABLE = ("-O2", "-march=x86-64")
NATIVE = ("-O2", "-march=skylake")
# Each edge changes one setting. The size trial overrides only the solver object.
CONFIGS = {
    "portable": PORTABLE,
    "portable-o3": ("-O3", "-march=x86-64"),
    "tuned": (*PORTABLE, "-mtune=skylake"),
    "native": NATIVE,
    "native-o3": ("-O3", "-march=skylake"),
    "portable-lto": (*PORTABLE, "-flto"),
    "native-lto": (*NATIVE, "-flto"),
    "aligned": (*NATIVE, "-mbranches-within-32B-boundaries"),
    "solver-os": NATIVE,
}
EDGES = [("portable", "portable-o3"), ("portable", "tuned"), ("tuned", "native"),
         ("native", "native-o3"), ("portable", "portable-lto"), ("native", "native-lto"),
         ("portable", "portable-pgo"), ("native", "native-pgo"),
         ("native", "aligned"), ("native", "solver-os")]


def identity(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True).encode()).hexdigest()


def write_json(path, obj):
    tmp = path.with_suffix(path.suffix + ".tmp")
    tmp.write_text(json.dumps(obj, indent=2, sort_keys=True) + "\n")
    tmp.replace(path)


def build(name, flags, *, profile=None, full_registry=False):
    host = name == "host"
    cxx = shutil.which("clang++-18")
    version = subprocess.check_output([cxx, "--version"], text=True).strip()
    ld = subprocess.check_output(["ld", "--version"], text=True).splitlines()[0]
    common = ["-std=c++17", *flags, "-Iinclude", "-Ibench/window_counters", "-Ibench/measurement_support",
              "-DSCRABBLE_WINDOW_VARIANTS"]
    if not full_registry:
        common.append("-DSCRABBLE_TUNING_PAIR")
    if profile:
        common += [f"-fprofile-instr-use={profile}", "-Werror=profile-instr-out-of-date",
                   "-Werror=profile-instr-unprofiled"]
    sources = [*SOURCES, HERE / ("window_worker.cpp" if host else "tuning_library.cpp")]
    headers = [*Path("include").glob("*.h"), HERE / "window_variants.h",
               Path("bench/measurement_support/benchmark_solver.h")]
    if host:
        headers.append(HERE / "tuning_loader.h")
        common += ["-DWINDOW_TUNING_HOST", '-DWINDOW_BUILD_FLAGS="tuning host"']
    else:
        common += ["-fPIC", "-fvisibility=hidden", "-fvisibility-inlines-hidden"]
    product = "worker" if host else "counter.so"
    commands = []
    objects = []
    for source in sources:
        extra = []
        if source == SOLVER and not host:
            extra = ["-Rpass=inline|loop-vectorize", "-Rpass-missed=inline|loop-vectorize"]
            if name == "solver-os":
                extra.append("-Os")
        obj = "{out}/" + source.stem + ".o"
        commands.append([cxx, *common, *extra, "-c", str(source), "-o", obj])
        objects.append(obj)
    link = ["-ldl"] if host else ["-shared", "-Wl,-z,defs"]
    commands.append([cxx, *common, *objects, *link, "-o", "{out}/" + product])
    config = dict(compiler=version, linker=ld, inputs={str(p): sha256(p) for p in sources + headers},
                  commands=commands, profile_sha256=sha256(profile) if profile else None)
    directory = WORK / "builds" / identity(config)[:20]
    directory.mkdir(parents=True, exist_ok=True)
    binary = directory / product
    stamp = directory / "build.json"
    if stamp.exists() and binary.exists():
        saved = json.loads(stamp.read_text())
        if sha256(binary) == saved["sha256"]:
            return saved
    print(f"build {name} in {directory}", flush=True)
    executed = []
    with gzip.open(directory / "remarks.txt.gz", "wt") as remarks:
        for cmd in commands:
            command = [arg.replace("{out}", str(directory)) for arg in cmd]
            executed.append(shlex.join(command))
            result = subprocess.run(command, text=True, stderr=subprocess.PIPE)
            remarks.write(result.stderr)
            if result.returncode:
                raise RuntimeError(f"{executed[-1]}\n{result.stderr[-5000:]}")
    text_bytes = int(next(line.split()[1] for line in subprocess.check_output(
        ["size", "-A", binary], text=True).splitlines() if line.startswith(".text ")))
    record = dict(config, commands=executed, binary=str(binary), sha256=sha256(binary), text_bytes=text_bytes)
    write_json(stamp, record)
    return record
