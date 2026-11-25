"""Exercise counter qualification and timing through the window benchmark."""
import csv
import io
import os
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

from benchmark_test_support import (
    FIXTURE, PROJECT_CACHE, WINDOW, record_worker_caches, seed_cache,
    stage_benchmark_code, stage_qualifier, window_build,
)


class WindowBenchmarkTest(unittest.TestCase):
    def test_window_times_one_board_and_rack(self):
        output = subprocess.check_output(
            [*WINDOW, "--board", FIXTURE / "crossings.csv",
             "--rack", "TR", "--repetitions", "1"], text=True)
        row, = csv.DictReader(io.StringIO(output))
        self.assertEqual(Path(row["board"]).resolve(), FIXTURE / "crossings.csv")
        self.assertEqual((row["rack"], row["across"], row["down"]), ("RT", "2", "1"))

    def test_paired_window_solves(self):
        output = subprocess.check_output(
            [*WINDOW, "--variants", "packed", "one-gap", "--repetitions", "2"],
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
        for command in ([*WINDOW, "--variants", "packed", "absent"],
                        [directory / "window_worker", "packed", "absent", "unused-dictionary", "unused-cache"]):
            result = subprocess.run(command, text=True, capture_output=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("unknown window counter: absent", result.stderr)
            self.assertEqual(result.stdout, "")

    def test_window_worker_loads_the_file_used_for_qualification(self):
        dictionary = FIXTURE / "dictionary.txt"
        binaries = window_build()
        with tempfile.TemporaryDirectory() as directory:
            outside = Path(directory)
            project = outside / "project"
            stage_benchmark_code(project)

            # The tools' own project has a matching cache even when the runner's project doesn't.
            tools = outside / "tools"
            build = tools / "build/window" / binaries.name
            qualifier = stage_qualifier(binaries, build)
            (build / "perpendicular_checks_test").symlink_to(binaries / "perpendicular_checks_test")
            paths = record_worker_caches(binaries / "window_worker", build)
            tools_cache = tools / PROJECT_CACHE
            tools_cache.parent.mkdir(parents=True)
            seed_cache(qualifier, dictionary, tools_cache, outside)

            project_cache = project / PROJECT_CACHE
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
                    result = subprocess.run([*WINDOW, "--build-dir", build,
                                             "--dictionary", dictionary, "--cases", FIXTURE / "cases.csv",
                                             "--repetitions", "1", *options], cwd=outside,
                                            env={**os.environ, "PYTHONPATH": str(project)}, capture_output=True, text=True)
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


if __name__ == "__main__":
    unittest.main()
