"""Stage benchmark runners and observe which cache their workers use."""
import os
import shutil
import subprocess
import sys
from pathlib import Path

WINDOW = [sys.executable, "-m", "bench.window_counters.window_benchmark"]
PAIRED = [sys.executable, "-m", "bench.validator_comparison.paired_benchmark"]
PROJECT_CACHE = Path("data/caches/pattern_and_rack_anagram_to_words.cache")
FIXTURE = Path("bench/fixtures/paired").resolve()


def window_build():
    directory = os.environ.get("WINDOW_BUILD") or subprocess.check_output(
        ["make", "-s", "--no-print-directory", "window-tools"], text=True).strip()
    return Path(directory).resolve()


def stage_qualifier(binaries, directory):
    directory.mkdir(parents=True)
    qualifier = directory / "placement_equivalence_test"
    qualifier.symlink_to(binaries / qualifier.name)
    return qualifier


def seed_cache(qualifier, dictionary, cache, cwd):
    subprocess.run([qualifier, dictionary, cache], cwd=cwd, input=b"", check=True, capture_output=True)


def record_worker_caches(binary, directory):
    # Paired and window workers both take the timing cache as their fourth argument.
    worker = directory / binary.name
    worker.with_suffix(".native").symlink_to(binary)
    worker.write_text('#!/bin/sh\ncase "$1" in --*) ;; *) printf "%s\\0" "$4" >> "$0.paths" ;; esac\n'
                      'exec "$0.native" "$@"\n')
    worker.chmod(0o755)
    return worker.with_suffix(".paths")


def stage_benchmark_code(project):
    shutil.copytree("bench", project / "bench",
                    ignore=shutil.ignore_patterns("fixtures", "results", "__pycache__"))
