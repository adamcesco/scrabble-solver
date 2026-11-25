"""Exercise counter qualification and timing through the window benchmark."""
import csv
import io
import os
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

from benchmark_test_support import (
    FIXTURE, PROJECT_CACHE, WINDOW, record_worker_caches, seed_cache,
    stage_benchmark_code, stage_qualifier, window_build,
)

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from bench.window_counters.window_profile import summarize


class WindowBenchmarkTest(unittest.TestCase):
    def test_reserved_game_lists_reproduce_and_export_disjoint_cases(self):
        command = ["python3", "-m", "bench.window_counters.window_games"]
        manifest = Path("bench/fixtures/window-game-sets.csv")
        self.assertEqual(subprocess.check_output(command), manifest.read_bytes())
        with manifest.open() as stream:
            members = list(csv.DictReader(stream))
        self.assertEqual(len({r["game"] for r in members}), len(members))
        with tempfile.TemporaryDirectory() as directory:
            for split, per_stage in (("development", 10), ("pgo", 16), ("held-out", 32)):
                dest = Path(directory) / split
                subprocess.run([*command, "--export", split, dest], check=True)
                with (dest / "cases.csv").open() as stream:
                    cases = list(csv.DictReader(stream))
                expected = [(r["case"], r["stage"]) for r in members if r["set"] == split]
                self.assertEqual([(r["case"], r["stage"]) for r in cases], expected)
                for stage in ("early", "middle", "late"):
                    self.assertEqual(sum(r["stage"] == stage for r in cases), per_stage)

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
                 Path(directory) / "patterns", "packed", "one-gap", "empty-bits", "neighbor-bits"],
                input=f"{fixture / 'crossings.csv'}\tRT\n", text=True)
            self.assertEqual(list(csv.reader(io.StringIO(counts)))[1:], [["2", "4", "1", "5"]])

    def test_workload_counts_individually_checked_words_after_group_shortcuts(self):
        with tempfile.TemporaryDirectory() as directory:
            profile = Path(directory) / "visits.csv"
            counts = subprocess.check_output(
                [window_build() / "placement_equivalence_test", FIXTURE / "dictionary.txt",
                 Path(directory) / "patterns", "--profile", profile, "packed", "one-gap"],
                input=f"{FIXTURE / 'crossings.csv'}\tRT\n", text=True)
            self.assertEqual(list(csv.reader(io.StringIO(counts)))[1:], [["2", "4", "1", "5"]])
            raw_visits = profile.read_text()
        visits = list(csv.DictReader(io.StringIO(raw_visits)))
        variants = ("packed", "one-gap")
        for variant in variants:
            rows = [r for r in visits if r["variant"] == variant]
            self.assertEqual(sum(int(r["group_size"]) for r in rows), 9)
            self.assertTrue(any(r["group_size"] == "0" and r["pattern_match"] == "1" for r in rows))
            self.assertTrue(any(r["pattern_match"] == "0" for r in rows))
            # RAT and TAR share the RT group at .A. across. Their order matters.
            group, = [r for r in rows if (r["down"], r["row"], r["start"], r["length"]) == ("0", "8", "6", "3")]
            self.assertEqual((group["empty"], group["constrained"], group["group_size"], group["individual_checks"]),
                             ("2", "2", "2", "2"))
            single = [r for r in rows if r["empty"] == "1" and r["group_size"] != "0"]
            self.assertTrue(single)
            self.assertEqual(any(int(r["individual_checks"]) for r in single), variant == "packed")

        cases = [{"case": "RT", "stage": "early"}]
        stage = summarize(io.StringIO(raw_visits), cases, variants, [{"across_candidates": 4, "down_candidates": 5}])["early"]
        self.assertEqual(stage["variants"]["one-gap"]["groups_by_size"][2],
                         {"groups": 2, "candidate_words": 4, "individual_checks": 2})
        self.assertEqual(stage["variants"]["packed"]["windows"]["matching_groups"]["by_candidates"], {1: 1, 2: 4})
        # Matching the combined total is insufficient if either direction is wrong.
        with self.assertRaisesRegex(ValueError, "RT packed across"):
            summarize(io.StringIO(raw_visits), cases, variants, [{"across_candidates": 5, "down_candidates": 4}])

    def test_neighbor_lookups_at_both_edges_in_both_views(self):
        variants = ("packed", "empty-bits", "neighbor-bits")
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            # Every crossing with A permits all letters, even when a lookup is needed.
            words = {"ST", "TS"}
            for char in "ABCDEFGHIJKLMNOPQRSTUVWXYZ":
                words.update(("A" + char, char + "A"))
            (root / "words").write_text("\n".join(sorted(words)) + "\n")
            board = [["."] * 15 for _ in range(15)]
            board[1][1] = board[13][13] = "A"
            with (root / "board.csv").open("w") as stream:
                csv.writer(stream, lineterminator="\n").writerows(board)
            output = subprocess.check_output(
                [window_build() / "placement_equivalence_test", root / "words", root / "cache",
                 "--profile", root / "visits", *variants], input=f"{root / 'board.csv'}\tST\n", text=True)
            qualified = {k: int(v) for k, v in next(csv.DictReader(io.StringIO(output))).items()}
            self.assertEqual(qualified["across"], 24)
            self.assertEqual(qualified["down"], 24)
            profile = (root / "visits").read_text()
        cases = [{"case": "two As", "stage": "early"}]
        stage = summarize(io.StringIO(profile), cases, variants, [qualified])["early"]
        for name in variants:
            filtered = name == "neighbor-bits"
            for direction in ("across", "down"):
                self.assertEqual(stage["variants"][name]["lookup_counts"][direction],
                                 dict(calls=8 if filtered else 20, matched_windows=12,
                                      matched_windows_without_lookups=4 if filtered else 0))
            edges = [r for r in csv.DictReader(io.StringIO(profile)) if r["variant"] == name
                     and r["row"] in ("0", "14") and r["group_size"] != "0"]
            self.assertEqual({(r["row"], r["down"]) for r in edges},
                             {("0", "0"), ("14", "0"), ("0", "1"), ("14", "1")})
            self.assertTrue(all(r["constrained"] == "0" and int(r["lookups"]) == (1 if filtered else 2)
                                for r in edges))

    def test_workload_output_cannot_overwrite_qualification_inputs(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            dictionary = root / "words"
            board = root / "board.csv"
            cache = root / "patterns"
            shutil.copyfile(FIXTURE / "dictionary.txt", dictionary)
            shutil.copyfile(FIXTURE / "crossings.csv", board)
            qualifier = window_build() / "placement_equivalence_test"
            seed_cache(qualifier, dictionary, cache, root)
            output = root / "visits.csv"
            command = [qualifier, dictionary, cache, "--load-cache", "--profile", output, "packed"]
            for path in (dictionary, board, cache):
                with self.subTest(path=path.name):
                    original = path.read_bytes()
                    output.hardlink_to(path)
                    result = subprocess.run(command, input=f"{board}\tRT\n", text=True, capture_output=True)
                    output.unlink()
                    self.assertEqual(path.read_bytes(), original)
                    self.assertNotEqual(result.returncode, 0)
                    self.assertIn("profile output aliases", result.stderr)
            for _ in range(2):
                subprocess.run(command, input=f"{board}\tRT\n", text=True, capture_output=True, check=True)
                self.assertTrue(output.read_text().startswith("position,variant,"))

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
