"""Qualify full-dictionary v3 builds and reads against the sample board."""

import argparse
import csv
import json
import subprocess
import tempfile
from pathlib import Path

from bench.measurement_support.benchmark_inputs import sha256
from bench.measurement_support.rack_neighbor_build import FLAGS, build_worker

from .reference import CACHE, DICTIONARY, REVISION, ROOT

BOARD = "boards/sample_board.csv"
EXPECTED_INPUTS = {
    DICTIONARY: "559faa1e0296166f8f4c6074b71211c045202accb6920424aaa0758cc7bce9cd",
    BOARD: "8caacd03aa304f12e83c0eb527d3520144d779dfa555c92ed7f7d304bde56a47",
}
DRIVER = Path(__file__).with_suffix(".cpp")


def qualify(data, output, production):
    for relative, expected in EXPECTED_INPUTS.items():
        if sha256(data / relative) != expected:
            raise ValueError(f"unexpected full qualification input: {relative}")
    production = subprocess.check_output(["git", "rev-parse", "--verify", f"{production}^{{commit}}"],
                                         cwd=ROOT, text=True).strip()
    if output.exists():
        raise FileExistsError(output)
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=output.parent) as temporary:
        temp = Path(temporary)
        build_worker(temp / "production", production, DRIVER)
        build_worker(temp / "pinned", REVISION, DRIVER)
        prod = temp / "production/worker"
        old = temp / "pinned/worker"
        fresh = temp / "fresh.cache"
        existing = data / CACHE
        results = []
        for label, worker, revision, mode, cache in [
            ("existing", prod, production, "load", existing),
            ("fresh", prod, production, "build", fresh),
            ("fresh", prod, production, "load", fresh),
            ("fresh", old, REVISION, "load", fresh),
        ]:
            print(f"{revision[:7]} {mode} {label} cache", flush=True)
            timing = temp / "time.txt"
            command = ["/usr/bin/time", "-f", "%e %M", "-o", timing, worker, mode, data / DICTIONARY, cache, data / BOARD]
            fields = subprocess.check_output(command, text=True).strip().split(",")
            build, save, load, across, down = fields
            if (across, down) != ("254", "102"):
                raise ValueError(f"{revision} {mode} {label}: got {across} across and {down} down")
            elapsed, peak = timing.read_text().split()
            results.append(dict(revision=revision, cache=label, operation=mode, across=across, down=down,
                                build_s=build, save_s=save, load_s=load, wall_s=elapsed, peak_rss_kib=peak))

        provenance = {
            "rack": "ABFHIKU", "inputs_sha256": EXPECTED_INPUTS,
            "caches": {name: {"bytes": path.stat().st_size, "sha256": sha256(path)}
                       for name, path in [("existing", existing), ("fresh", fresh)]},
            "compiler": subprocess.check_output(["clang++", "--version"], text=True).splitlines()[0],
            "flags": FLAGS, "driver_sha256": sha256(DRIVER),
        }
        report = temp / "report"
        report.mkdir()
        with (report / "observations.csv").open("w", newline="") as stream:
            writer = csv.DictWriter(stream, fieldnames=results[0], lineterminator="\n")
            writer.writeheader()
            writer.writerows(results)
        (report / "provenance.json").write_text(json.dumps(provenance, indent=2) + "\n")
        report.rename(output)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("data", type=Path, help="directory containing the full sample dictionary, board and existing cache")
    parser.add_argument("output", type=Path, help="new report directory; its filesystem must also fit a fresh cache")
    parser.add_argument("--production", default="HEAD", help="production commit to compile")
    args = parser.parse_args()
    qualify(args.data.resolve(), args.output.resolve(), args.production)
