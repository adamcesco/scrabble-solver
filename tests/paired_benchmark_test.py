"""Exercise the paired report through the same runner used for larger manifests."""
import csv
import io
import os
import shutil
import subprocess
import tempfile
import unittest
from collections import defaultdict
from pathlib import Path


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


class PairedBenchmarkTest(unittest.TestCase):
    def test_paired_window_solves(self):
        output = subprocess.check_output(
            ["python3", "bench/window_benchmark.py", "--variants", "packed", "one-gap", "--repetitions", "2"],
            text=True)
        rows = list(csv.DictReader(io.StringIO(output)))
        self.assertEqual([r["first_variant"] for r in rows], ["packed", "one-gap"])
        for row in rows:
            self.assertEqual((row["across"], row["down"], row["rack"]), ("6", "6", "ART"))
            self.assertEqual((row["baseline"], row["challenger"]), ("packed", "one-gap"))
            baseline, challenger = float(row["baseline_solve_us"]), float(row["challenger_solve_us"])
            self.assertGreater(min(baseline, challenger), 0)
            self.assertAlmostEqual(float(row["saving_us"]), baseline - challenger)

    def test_qualify_named_window_counters_on_same_input(self):
        fixture = Path("bench/fixtures/paired")
        with tempfile.TemporaryDirectory() as directory:
            counts = subprocess.check_output(
                ["build/placement_equivalence_test", fixture / "dictionary.txt",
                 Path(directory) / "patterns", "packed", "one-gap"],
                input=f"{fixture / 'crossings.csv'}\tRT\n", text=True)
            self.assertEqual(list(csv.reader(io.StringIO(counts)))[1:], [["2", "4", "1", "5"]])

    def test_unknown_window_counter_prevents_timing(self):
        directory = window_build()
        for command in (["python3", "bench/window_benchmark.py", "--variants", "packed", "absent"],
                        [directory / "window_worker", "packed", "absent", "unused-dictionary", "unused-cache"]):
            result = subprocess.run(command, text=True, capture_output=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("unknown window counter: absent", result.stderr)
            self.assertEqual(result.stdout, "")

    def test_standalone_qualification_finds_matching_project_cache(self):
        dictionary = FIXTURE / "dictionary.txt"
        with tempfile.TemporaryDirectory() as directory:
            outside = Path(directory)
            root = outside / "project"
            cache = root / PROJECT_CACHE
            cache.parent.mkdir(parents=True)
            nested = window_build()
            flat = stage_qualifier(Path("build").resolve(), root / "build")
            window = stage_qualifier(nested, root / "build/window" / nested.name)
            seed_cache(flat, dictionary, cache, outside)
            modified = cache.stat().st_mtime_ns
            for qualifier in (flat, window):
                with self.subTest(layout=qualifier.parent.relative_to(root)):
                    private = qualifier.parent / "private-patterns"
                    result = subprocess.run([qualifier, dictionary, private], cwd=outside,
                                            input=f"{FIXTURE / 'crossings.csv'}\tRT\n", text=True,
                                            check=True, capture_output=True)
                    counts, = list(csv.reader(io.StringIO(result.stdout)))[1:]
                    self.assertEqual(counts, ["2", "4", "1", "5"])
                    self.assertFalse(private.exists(), f"returned {','.join(counts)} but created {private}")
                    self.assertEqual(cache.stat().st_mtime_ns, modified)

    def test_standalone_qualification_builds_on_miss_and_preserves_an_invalid_explicit_cache(self):
        dictionary = FIXTURE / "dictionary.txt"
        with tempfile.TemporaryDirectory() as directory:
            outside = Path(directory)
            root = outside / "project"
            qualifier = stage_qualifier(Path("build").resolve(), root / "build")
            project_cache = root / PROJECT_CACHE
            project_cache.parent.mkdir(parents=True)
            other = outside / "other-dictionary"
            other.write_bytes(dictionary.read_bytes() + b"\nZZ\n")
            for case in ("absent", "mismatched"):
                with self.subTest(case=case):
                    if case == "mismatched":
                        seed_cache(qualifier, other, project_cache, outside)
                    private = outside / case
                    seed_cache(qualifier, dictionary, private, outside)
                    probe = subprocess.check_output([Path("build/paired_worker").resolve(), "--check-cache",
                                                     dictionary, private], text=True, cwd=outside)
                    self.assertEqual(probe.strip(), "match")

            # Offer a matching project cache alongside a caller-supplied file that cannot load.
            shutil.copyfile(private, project_cache)
            explicit = outside / "explicit"
            explicit.write_bytes(b"invalid")
            result = subprocess.run([qualifier, dictionary, explicit], cwd=outside, input=b"", capture_output=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertEqual(explicit.read_bytes(), b"invalid")

    def test_window_worker_loads_the_file_used_for_qualification(self):
        dictionary = FIXTURE / "dictionary.txt"
        binaries = window_build()
        with tempfile.TemporaryDirectory() as directory:
            outside = Path(directory)
            runner = outside / "project/bench"
            runner.mkdir(parents=True)
            for script in ("paired_benchmark.py", "window_benchmark.py"):
                shutil.copyfile(Path("bench") / script, runner / script)

            # The tools' own project has a matching cache even when the runner's project doesn't.
            tools = outside / "tools"
            build = tools / "build/window" / binaries.name
            qualifier = stage_qualifier(binaries, build)
            (build / "perpendicular_checks_test").symlink_to(binaries / "perpendicular_checks_test")
            paths = record_worker_caches(binaries / "window_worker", build)
            tools_cache = tools / PROJECT_CACHE
            tools_cache.parent.mkdir(parents=True)
            seed_cache(qualifier, dictionary, tools_cache, outside)

            project_cache = runner.parent / PROJECT_CACHE
            project_cache.parent.mkdir(parents=True)
            explicit = outside / "explicit patterns"
            shutil.copyfile(tools_cache, explicit)
            for case in ("absent", "mismatched", "matching", "explicit"):
                with self.subTest(case=case):
                    options = []
                    project_cache.unlink(missing_ok=True)
                    if case == "mismatched":
                        other = outside / "other-dictionary"
                        other.write_bytes(dictionary.read_bytes() + b"\nZZ\n")
                        seed_cache(qualifier, other, project_cache, outside)
                    elif case in ("matching", "explicit"):
                        shutil.copyfile(tools_cache, project_cache)
                    if case == "explicit":
                        options = ["--cache", explicit]
                    before = project_cache.read_bytes() if project_cache.exists() else None
                    paths.write_bytes(b"")
                    result = subprocess.run(["python3", runner / "window_benchmark.py", "--build-dir", build,
                                             "--dictionary", dictionary, "--cases", FIXTURE / "cases.csv",
                                             "--repetitions", "1", *options], cwd=outside, capture_output=True, text=True)
                    self.assertEqual(result.returncode, 0, result.stderr)
                    timed_path, terminator = paths.read_bytes().split(b"\0")
                    self.assertEqual(terminator, b"")
                    timed = Path(os.fsdecode(timed_path))
                    if case in ("matching", "explicit"):
                        self.assertEqual(timed, explicit if options else project_cache)
                        action = "loading"
                    else:
                        self.assertNotIn(timed, (tools_cache, project_cache, explicit))
                        action = "building"
                    self.assertIn(f'Qualification {action} "{timed}"', result.stderr)
                    self.assertEqual(project_cache.read_bytes() if project_cache.exists() else None, before)
                    row, = csv.DictReader(io.StringIO(result.stdout))
                    self.assertEqual((row["across"], row["down"]), ("6", "6"))

    def test_existing_pattern_cache_survives_qualification_and_timing(self):
        dictionary = FIXTURE / "dictionary.txt"
        with tempfile.TemporaryDirectory() as directory:
            project = Path(directory)
            runner = project / "bench/paired_benchmark.py"
            runner.parent.mkdir()
            shutil.copyfile(Path("bench") / runner.name, runner)
            build = project / "build"
            native = Path("build").resolve()
            qualifier = stage_qualifier(native, build)
            (build / "perpendicular_checks_test").symlink_to(native / "perpendicular_checks_test")
            paths = record_worker_caches(native / "paired_worker", build)
            cache = project / PROJECT_CACHE
            cache.parent.mkdir(parents=True)
            seed_cache(qualifier, dictionary, cache, project)
            modified = cache.stat().st_mtime_ns
            for condition, options in [("hit", ["--cache", cache]), ("miss", ["--condition", "miss"])]:
                with self.subTest(condition=condition):
                    paths.write_bytes(b"")
                    result = subprocess.run(["python3", runner, "--dictionary", dictionary, "--cases",
                                             FIXTURE / "cases.csv", "--repetitions", "1", *options],
                                            capture_output=True, text=True, check=True)
                    self.assertIn(f'Qualification loading "{cache}"', result.stderr)
                    self.assertEqual(cache.stat().st_mtime_ns, modified)
                    self.assertEqual([r["cache"] for r in csv.DictReader(io.StringIO(result.stdout))], [condition] * 3)
                    first, second, _ = paths.read_bytes().split(b"\0")
                    if condition == "hit":
                        self.assertEqual((first, second), (os.fsencode(cache), os.fsencode(cache)))
                    else:
                        self.assertEqual(len({Path(os.fsdecode(p)).parent for p in (first, second)}), 2)
                        self.assertNotIn(os.fsencode(cache), (first, second))

    def test_illegal_second_board_prevents_any_timings(self):
        with tempfile.TemporaryDirectory() as directory:
            dictionary = Path(directory) / "words"
            # AS and SAT remain legal. The second board also contains TO.
            dictionary.write_text("AS\nSAT\nAR\nAT\nRAT\nTAR\n")
            result = subprocess.run(
                ["python3", "bench/paired_benchmark.py", "--dictionary", dictionary],
                capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("illegal run TO", result.stderr)
            self.assertEqual(result.stdout, "")

    def test_fixture_report(self):
        output = subprocess.check_output(
            ["python3", "bench/paired_benchmark.py", "--repetitions", "2"], text=True)
        groups = defaultdict(list)
        for row in csv.DictReader(io.StringIO(output)):
            groups[row["cache"], row["workload"]].append(row)

        for cache in ("hit", "miss"):
            for workload in ("first_board", "new_board", "new_rack"):
                rows = groups.pop((cache, workload))
                self.assertEqual([r["first_validator"] for r in rows], ["original", "packed"])
                for r in rows:
                    expected = {"first_board": (6, 6, 28, 26), "new_board": (2, 1, 4, 5),
                                "new_rack": (3, 1, 6, 7)}
                    self.assertEqual(tuple(int(r[key]) for key in (
                        "across", "down", "across_candidates", "down_candidates")), expected[workload])
                    for variant in ("original", "packed"):
                        def ms(phase):
                            return float(r[f"{variant}_{phase}_ms"])
                        self.assertGreater(ms("solve"), 0)
                        self.assertGreaterEqual(ms("total"), ms("solve"))
                        self.assertEqual(ms("dictionary") > 0, workload == "first_board")
                        self.assertEqual(ms("pattern_load") > 0, workload == "first_board")
                        self.assertEqual(ms("pattern_build") > 0, workload == "first_board" and cache == "miss")
                        self.assertEqual(ms("pattern_save") > 0, workload == "first_board" and cache == "miss")
                        self.assertEqual(ms("board") > 0, workload != "new_rack")
                        self.assertEqual(ms("run_starts") > 0, workload != "new_rack" and variant == "packed")
                    self.assertAlmostEqual(float(r["saving_ms"]),
                                           float(r["original_total_ms"]) - float(r["packed_total_ms"]))
        self.assertFalse(groups)


if __name__ == "__main__":
    unittest.main()
