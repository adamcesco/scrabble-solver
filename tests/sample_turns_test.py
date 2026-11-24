"""Check the saved timing pilot through the sampler and recorded-game commands."""
import csv
import hashlib
import io
import json
import subprocess
import sys
import tempfile
import unittest
from collections import Counter
from pathlib import Path

PILOT = Path("bench/fixtures/pilot")


def sample(*args):
    return subprocess.check_output([sys.executable, "bench/sample_turns.py",
                                    "--dictionary", PILOT / "words.txt", *args], text=True)


class SampleTurnsTest(unittest.TestCase):
    def test_reproduces_the_saved_pilot_and_replays_every_turn(self):
        saved = json.loads((PILOT / "corpus.json").read_text())
        settings = ("--seed", str(saved["seed"]))
        first = sample(*settings)
        self.assertEqual(first, sample(*settings))
        corpus = json.loads(first)
        dictionary = PILOT / "words.txt"
        fingerprint = hashlib.sha256(dictionary.read_bytes()).hexdigest()
        for case in corpus["cases"]:
            replay = json.loads(subprocess.check_output([
                sys.executable, "bench/replay_game.py", "--game", PILOT / case["game"],
                "--dictionary", dictionary, "--turn", str(case["turn"])]))
            self.assertEqual((case["board"], case["rack"]), (replay["board"], replay["rack"]))
            self.assertEqual(case["dictionary_sha256"], fingerprint)
            # The committed sample uses the full timing dictionary. A clean checkout
            # can replay the same legal games with the checked-in 252-word subset.
            case["dictionary_sha256"] = "559faa1e0296166f8f4c6074b71211c045202accb6920424aaa0758cc7bce9cd"
        self.assertEqual(corpus, saved)

    def test_balances_games_by_thirds_of_their_eligible_turns(self):
        # Last recorded turn in each game's early and middle thirds. Blank racks
        # leave gaps, particularly between turns 7 and 17 in the second game.
        bounds = {"sjoholm-del-solar": (8, 14), "6zTBhes669": (6, 21),
                  "7bbdiRsCRY": (8, 15), "CpZHNqsaAe": (8, 17),
                  "NSFqNG8yP5": (10, 17), "RHaUnC67uz": (9, 18)}
        for seed in range(6):
            corpus = json.loads(sample("--seed", str(seed)))
            self.assertEqual(corpus["case_count"], 6)
            counts = {"early": 2, "middle": 2, "late": 2}
            self.assertEqual(corpus["stage_counts"], counts)
            self.assertEqual(Counter(c["stage"] for c in corpus["cases"]), counts)
            self.assertCountEqual([Path(c["game"]).stem for c in corpus["cases"]], bounds.keys())
            for case in corpus["cases"]:
                early_end, middle_end = bounds[Path(case["game"]).stem]
                expected = ("early" if case["turn"] <= early_end else
                            "middle" if case["turn"] <= middle_end else "late")
                self.assertEqual(case["stage"], expected, case["game"])

    def test_exports_the_pilot_for_all_paired_workloads(self):
        with tempfile.TemporaryDirectory() as directory:
            corpus = json.loads(sample("--benchmark-dir", directory))
            manifest = Path(directory) / "cases.csv"
            with manifest.open() as stream:
                rows = list(csv.DictReader(stream))
            self.assertEqual(len(rows), len(corpus["cases"]))
            for row, case in zip(rows, corpus["cases"], strict=True):
                self.assertEqual((row["stage"], row["rack"]), (case["stage"], case["rack"]))
                with (manifest.parent / row["board"]).open() as stream:
                    self.assertEqual(["".join(line) for line in csv.reader(stream)],
                                     [line.upper() for line in case["board"]])
                companion = next(other for other in rows if other["board"] == row["new_board"])
                self.assertNotEqual(row["board"], companion["board"])
                self.assertEqual((row["stage"], row["new_board_rack"]),
                                 (companion["stage"], companion["rack"]))
            timings = subprocess.check_output(
                [sys.executable, "bench/paired_benchmark.py", "--cases", manifest,
                 "--dictionary", PILOT / "words.txt", "--repetitions", "1"], text=True)
            self.assertEqual(len(list(csv.DictReader(io.StringIO(timings)))), 36)


if __name__ == "__main__":
    unittest.main()
