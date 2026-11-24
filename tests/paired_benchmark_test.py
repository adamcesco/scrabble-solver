"""Exercise the paired report through the same runner used for larger manifests."""
import csv
import io
import subprocess
import tempfile
import unittest
from collections import defaultdict
from pathlib import Path


class PairedBenchmarkTest(unittest.TestCase):
    def test_standalone_qualification_finds_matching_project_cache(self):
        dictionary = Path("bench/fixtures/paired/dictionary.txt")
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            qualifier = root / "build/placement_equivalence_test"
            qualifier.parent.mkdir()
            qualifier.symlink_to(Path("build/placement_equivalence_test").resolve())
            cache = root / "data/caches/pattern_and_rack_anagram_to_words.cache"
            cache.parent.mkdir(parents=True)
            subprocess.run([qualifier, dictionary, cache], check=True, capture_output=True, input=b"")
            modified = cache.stat().st_mtime_ns
            unused = root / "private-patterns"
            result = subprocess.run([qualifier, dictionary, unused], check=True, capture_output=True,
                                    input="bench/fixtures/paired/crossings.csv\tRT\n", text=True)
            self.assertIn("2,4,1,5", result.stdout)
            self.assertFalse(unused.exists())
            self.assertEqual(cache.stat().st_mtime_ns, modified)

    def test_existing_pattern_cache_survives_qualification_and_timing(self):
        fixture = Path("bench/fixtures/paired")
        with tempfile.TemporaryDirectory() as directory:
            cache = Path(directory) / "patterns"
            subprocess.run(["build/placement_equivalence_test", fixture / "dictionary.txt", cache],
                           input="", text=True, stdout=subprocess.DEVNULL, check=True)
            modified = cache.stat().st_mtime_ns
            output = subprocess.check_output(["python3", "bench/paired_benchmark.py",
                                              "--cache", cache, "--repetitions", "1"], text=True)
            self.assertEqual(cache.stat().st_mtime_ns, modified)
            self.assertEqual([r["cache"] for r in csv.DictReader(io.StringIO(output))],
                             ["hit"] * 3)

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
