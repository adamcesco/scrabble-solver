"""Exercise the paired validator report and the shared manifest rules."""
import csv
import io
import os
import shutil
import subprocess
import tempfile
import unittest
from collections import defaultdict
from pathlib import Path

from benchmark_test_support import (
    FIXTURE, PAIRED, PROJECT_CACHE, WINDOW, record_worker_caches, seed_cache,
    stage_benchmark_code, stage_qualifier, window_build,
)


class PairedBenchmarkTest(unittest.TestCase):
    def test_only_paired_timing_requires_companion_fields(self):
        with tempfile.TemporaryDirectory() as directory:
            manifest = Path(directory) / "cases.csv"
            (manifest.parent / "board.csv").symlink_to(FIXTURE / "opening.csv")
            for header, values in [("", ""), (",new_board,new_board_rack,additional_rack", ",missing.csv,RT,RST")]:
                with self.subTest(header=header):
                    manifest.write_text(f"case,stage,board,rack{header}\nopening,early,board.csv,TRA{values}\n")
                    args = ["--cases", manifest, "--repetitions", "1"]
                    output = subprocess.check_output([*WINDOW, *args], text=True)
                    row, = csv.DictReader(io.StringIO(output))
                    self.assertEqual((row["case"], row["stage"], row["rack"], row["across"], row["down"]),
                                     ("opening", "early", "ART", "6", "6"))
                    paired = subprocess.run([*PAIRED, *args],
                                            text=True, capture_output=True)
                    self.assertNotEqual(paired.returncode, 0)
                    self.assertIn("missing columns: new_board" if not header else "missing.csv", paired.stderr)
                    self.assertEqual(paired.stdout, "")

    def test_invalid_manifest_rack_identifies_the_case_and_column(self):
        with tempfile.TemporaryDirectory() as directory:
            manifest = Path(directory) / "cases.csv"
            manifest.write_text("case,stage,board,rack,new_board,new_board_rack,additional_rack\n"
                                f"bad-reuse,early,{FIXTURE / 'opening.csv'},ART,{FIXTURE / 'crossings.csv'},RT,?\n")
            result = subprocess.run([*PAIRED, "--cases", manifest],
                                    text=True, capture_output=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("bad-reuse", result.stderr)
            self.assertIn("additional_rack", result.stderr)
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
            shutil.copyfile(outside / "absent", project_cache)
            explicit = outside / "explicit"
            explicit.write_bytes(b"invalid")
            result = subprocess.run([qualifier, dictionary, explicit], cwd=outside, input=b"", capture_output=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertEqual(explicit.read_bytes(), b"invalid")

    def test_existing_pattern_cache_survives_qualification_and_timing(self):
        dictionary = FIXTURE / "dictionary.txt"
        with tempfile.TemporaryDirectory() as directory:
            project = Path(directory)
            stage_benchmark_code(project)
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
                    result = subprocess.run([*PAIRED, "--dictionary", dictionary, "--cases",
                                             FIXTURE / "cases.csv", "--repetitions", "1", *options],
                                            cwd=project, capture_output=True, text=True, check=True)
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
                [*PAIRED, "--dictionary", dictionary],
                capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("illegal run TO", result.stderr)
            self.assertEqual(result.stdout, "")

    def test_fixture_report(self):
        output = subprocess.check_output(
            [*PAIRED, "--repetitions", "2"], text=True)
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
