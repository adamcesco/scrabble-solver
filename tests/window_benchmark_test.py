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


def profile_board(board, words, rack, variants, *, check_release=False):
    with tempfile.TemporaryDirectory() as directory:
        root = Path(directory)
        (root / "words").write_text("\n".join(sorted(words)) + "\n")
        with (root / "board.csv").open("w") as stream:
            csv.writer(stream, lineterminator="\n").writerows(board)
        output = subprocess.check_output(
            [window_build() / "placement_equivalence_test", root / "words", root / "cache",
             "--profile", root / "visits", *variants], input=f"{root / 'board.csv'}\t{rack}\n", text=True)
        counts = {k: int(v) for k, v in next(csv.DictReader(io.StringIO(output))).items()}
        if check_release:
            # The paired runner checks both release counts against the independent qualifier.
            subprocess.run([*WINDOW, "--build-dir", window_build(), "--variants", *variants,
                            "--board", root / "board.csv", "--rack", rack, "--dictionary", root / "words",
                            "--cache", root / "cache", "--repetitions", "1"], check=True, stdout=subprocess.DEVNULL)
        return counts, (root / "visits").read_text()


def window_groups(visits, *, row, start, length, down=False):
    coordinates = (str(int(down)), str(row), str(start), str(length))
    return [r for r in csv.DictReader(io.StringIO(visits))
            if (r["down"], r["row"], r["start"], r["length"]) == coordinates]


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

    def test_eager_masks_for_word_pattern_with_no_matching_rack_subset(self):
        board = [["."] * 15 for _ in range(15)]
        board[1][0] = "A"
        _, visits = profile_board(board, ["ART"], "ZZ", ["packed", "eager"])
        target = window_groups(visits, row=1, start=0, length=3)
        self.assertEqual([(r["variant"], r["pattern_match"], r["group_size"], r["masks"], r["lookups"])
                          for r in target], [("packed", "1", "0", "0", "0"), ("eager", "1", "0", "1", "2")])

    def test_first_impossible_gap_skips_only_the_remaining_lookups(self):
        board = [["."] * 15 for _ in range(15)]
        board[0][:2], board[1][0] = ["A", "X"], "A"
        _, visits = profile_board(board, ["AX", "AA", "ART", "AST"], "RST", ["packed", "stop-gap"])
        target = window_groups(visits, row=1, start=0, length=3)
        self.assertEqual([(r["variant"], r["lookups"], r["group_size"]) for r in target],
                         [("packed", "2", "1"), ("packed", "2", "1"),
                          ("stop-gap", "1", "1"), ("stop-gap", "1", "1")])

    def test_window_stop_and_later_matching_groups_in_both_traversals(self):
        board = [["."] * 15 for _ in range(15)]
        board[0][:2], board[1][0] = ["A", "X"], "A"
        for binary in (False, True):
            for impossible in (False, True):
                with self.subTest(binary=binary, impossible=impossible):
                    words = {"AA", "AX", "ARS", "ART", "ATR", "AST", "ATS"}
                    if binary:
                        # More than four groups per two-letter rack subset selects binary probes.
                        words.update("A" + c + c for c in "BCDEFGHIJKLM")
                    if not impossible:
                        words.add("XT")  # RS fails, then RT and ST each have a passing word.
                    variants = ["stop-gap", "stop-window"]
                    counts, visits = profile_board(board, words, "RST", variants, check_release=True)
                    groups = [r for r in window_groups(visits, row=1, start=0, length=3)
                              if r["variant"] == "stop-window"]
                    self.assertEqual([int(r["group_size"]) for r in groups], [1, 2, 2])
                    self.assertEqual([int(r["skipped_group"]) for r in groups], [0, impossible, impossible])
                    self.assertEqual([int(r["accepted"]) for r in groups],
                                     [0, int(not impossible), int(not impossible)])
                    self.assertEqual(int(groups[0]["probes_avoided"]), 2 if binary and impossible else 0)
                    self.assertEqual(int(groups[0]["merge_comparisons_avoided"]), 2 if not binary and impossible else 0)
                    stage = summarize(io.StringIO(visits), [{"case": "A..", "stage": "early"}], variants, [counts])["early"]
                    search = stage["variants"]["stop-window"]
                    visited = sum(bucket["candidate_words"] for bucket in search["groups_by_size"].values())
                    skipped = search["mask_search"]["all_rejected"]["skipped_words"]
                    self.assertEqual(visited + skipped, counts["across_candidates"] + counts["down_candidates"])

    def test_whole_rack_drops_satisfied_gaps_and_stops_at_disjoint_check_set(self):
        board = [["."] * 15 for _ in range(15)]
        board[0][:2], board[1][0] = ["A", "X"], "A"
        for possible in (False, True):
            with self.subTest(possible=possible):
                words = {"AA", "AX", "ARR", "ART", "ATR", "XZ"}
                if possible:
                    words.update(("XR", "XT"))
                _, visits = profile_board(board, words, "RRT", ["all-letters", "rack"], check_release=True)
                target = window_groups(visits, row=1, start=0, length=3)
                control = [r for r in target if r["variant"] == "all-letters"]
                self.assertEqual([int(r["individual_checks"]) for r in control], [1, 2])
                self.assertTrue(all(r["rack_gaps_removed"] == "1" and r["ruled_out"] == "0" for r in control))
                groups = [r for r in target if r["variant"] == "rack"]
                self.assertEqual([int(r["group_size"]) for r in groups], [1, 2])
                self.assertEqual([int(r["accepted"]) for r in groups], [1, 2] if possible else [0, 0])
                self.assertEqual([int(r["skipped_group"]) for r in groups], [0, int(not possible)])
                self.assertTrue(all(r["individual_checks"] == "0" for r in groups))
                self.assertEqual(int(groups[0]["rack_gaps_removed"]), 2 if possible else 0)
                self.assertEqual(int(groups[0]["rack_restricted_removed"]), int(possible))

    def test_later_rack_subsets_pass_after_disjoint_and_repeated_gap_rejections(self):
        board = [["."] * 15 for _ in range(15)]
        board[0][:3], board[1][0] = list("AXX"), "A"
        for binary in (False, True):
            with self.subTest(binary=binary):
                words = {"AA", "AXX", "XT", "ARRS", "ARST", "ATTR", "ATRT", "ATTT"}
                if binary:
                    # RRSTTT has six three-tile subsets. Over 24 groups selects binary probes.
                    words.update("A" + letter * 3 for letter in "ABCDEFGHIJKLMNOPQRSUVWXYZ")
                _, visits = profile_board(board, words, "RRSTTT", ["subset", "subset-capacity"], check_release=True)
                target = window_groups(visits, row=1, start=0, length=4)
                for variant in ("subset", "subset-capacity"):
                    groups = [r for r in target if r["variant"] == variant]
                    # RRS has no T, RST has one T for two crossings, RTT can play ATTR only.
                    # TTT fills both crossings in every order.
                    self.assertEqual([int(r["group_size"]) for r in groups], [1, 1, 2, 1])
                    self.assertEqual([int(r["accepted"]) for r in groups], [0, 0, 1, 1])
                    self.assertEqual([int(r["individual_checks"]) for r in groups],
                                     [0, int(variant == "subset"), 2, 0])
                    self.assertEqual([int(r["subset_disjoint"]) for r in groups], [1, 0, 0, 0])
                    self.assertEqual([int(r["subset_insufficient_capacity"]) for r in groups],
                                     [0, int(variant == "subset-capacity"), 0, 0])
                    self.assertEqual([int(r["subset_gaps_removed"]) for r in groups], [0, 0, 0, 2])
                    self.assertTrue(all(r["skipped_group"] == "0" for r in groups))

    def test_singleton_control_and_capacity_with_two_different_eligible_tiles(self):
        board = [["."] * 15 for _ in range(15)]
        board[0][:3], board[1][0] = list("AXX"), "A"
        variants = ["subset-multi", "subset-capacity"]
        counts, visits = profile_board(board, ["AA", "AXX", "XS", "XT", "ARRT", "ASTR"],
                                       "RRST", variants, check_release=True)
        target = window_groups(visits, row=1, start=0, length=4)
        for name in variants:
            groups = [r for r in target if r["variant"] == name]
            self.assertEqual([int(r["group_size"]) for r in groups], [1, 1])
            self.assertEqual([int(r["accepted"]) for r in groups], [0, 1])
            self.assertEqual([int(r["individual_checks"]) for r in groups], [int(name == "subset-multi"), 1])
        stage = summarize(io.StringIO(visits), [{"case": "AXX", "stage": "early"}], variants, [counts])["early"]
        singles = [stage["variants"][name]["groups_by_size"][1] for name in variants]
        self.assertEqual(singles[0]["subset_checks"], 0)
        self.assertGreaterEqual(singles[1]["capacity_rejections"], 1)

    def test_row_cache_profile_includes_overlaps_and_rows_without_requests(self):
        board = [["."] * 15 for _ in range(15)]
        board[0][:2] = ["A", "X"]
        variants = ["row-cache"]
        counts, visits = profile_board(board, ["AX", "AT", "CAT", "TO"], "ACT", variants)
        stage = summarize(io.StringIO(visits), [{"case": "AX", "stage": "early"}], variants, [counts])["early"]
        self.assertEqual(stage["variants"]["row-cache"]["row_cache"]["across"],
                         dict(requests=10, hits=6, distinct_squares_per_row={0: 14, 4: 1}))

    def test_neighbor_lookups_at_both_edges_in_both_views(self):
        variants = ("packed", "empty-bits", "neighbor-bits")
        # Every crossing with A permits all letters, even when a lookup is needed.
        words = {"ST", "TS"}
        for char in "ABCDEFGHIJKLMNOPQRSTUVWXYZ":
            words.update(("A" + char, char + "A"))
        board = [["."] * 15 for _ in range(15)]
        board[1][1] = board[13][13] = "A"
        qualified, profile = profile_board(board, words, "ST", variants)
        self.assertEqual(qualified["across"], 24)
        self.assertEqual(qualified["down"], 24)
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
