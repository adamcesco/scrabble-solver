"""Check the saved corpora through the sampler and recorded-game commands."""
import csv
import gzip
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
MASS = Path("bench/fixtures/mass")
TIMING_DICTIONARY = Path("data/dictionaries/sample_dictionary.txt")


def sample(*args):
    return subprocess.check_output([sys.executable, "bench/sample_turns.py",
                                    "--dictionary", PILOT / "words.txt", *args], text=True)


class SampleTurnsTest(unittest.TestCase):
    def test_saved_mass_corpus_has_one_distinct_pair_per_game(self):
        with gzip.open(MASS / "corpus.json.gz", "rt") as stream:
            corpus = json.load(stream)
        cases = corpus["cases"]
        self.assertEqual(corpus["case_count"], len(cases))
        self.assertEqual(len(cases), 10_000)
        self.assertEqual(len({case["game"] for case in cases}), len(cases))
        self.assertEqual(len({case["source"] for case in cases}), len(cases))
        pairs = {("".join(c["board"]).upper(), tuple(sorted(c["rack"]))) for c in cases}
        self.assertEqual(len(pairs), len(cases))
        self.assertEqual(Counter(c["stage"] for c in cases), corpus["stage_counts"])
        self.assertEqual(corpus["stage_counts"], {"early": 3334, "middle": 3333, "late": 3333})

    @unittest.skipUnless(TIMING_DICTIONARY.is_file(), "full timing dictionary is supplied separately")
    def test_replays_and_exports_the_full_saved_corpus(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            subprocess.run(["tar", "-xzf", MASS / "games.tar.gz", "-C", root], check=True)
            export = root / "paired"
            summary = subprocess.check_output([sys.executable, "bench/sample_turns.py",
                                               "--games", root / "games.csv", "--verify", MASS / "corpus.json.gz",
                                               "--benchmark-dir", export], text=True)
            self.assertIn("Verified 10000 cases in order", summary)
            with (export / "cases.csv").open() as stream:
                rows = list(csv.DictReader(stream))
            with gzip.open(MASS / "corpus.json.gz", "rt") as stream:
                cases = json.load(stream)["cases"]
            companions = {row["board"]: case for row, case in zip(rows, cases, strict=True)}
            for row, case in zip(rows, cases, strict=True):
                self.assertEqual((row["case"], row["rack"], row["stage"]),
                                 (f"{case['game']}#{case['turn']}", case["rack"], case["stage"]))
                other = companions[row["new_board"]]
                self.assertNotEqual("".join(case["board"]).upper(), "".join(other["board"]).upper())
                self.assertEqual((other["stage"], other["rack"]), (row["stage"], row["new_board_rack"]))
                self.assertTrue(row["additional_rack"])
                self.assertEqual(row["additional_rack"], other["rack"][:-1])

    def test_selects_the_requested_number_of_games(self):
        corpus = json.loads(sample("--size", "4"))
        self.assertEqual(corpus["case_count"], 4)
        self.assertEqual(len({case["game"] for case in corpus["cases"]}), 4)
        self.assertEqual(corpus["stage_counts"], {"early": 2, "middle": 1, "late": 1})

    def test_verification_replays_the_saved_seed_size_and_order(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "corpus.json.gz"
            corpus = json.loads(sample("--size", "5", "--seed", "12345"))
            path.write_bytes(gzip.compress(json.dumps(corpus).encode()))
            self.assertIn("Verified 5 cases", sample("--verify", path))
            corpus["cases"].reverse()
            path.write_bytes(gzip.compress(json.dumps(corpus).encode()))
            with self.assertRaises(subprocess.CalledProcessError) as caught:
                subprocess.check_output([sys.executable, "bench/sample_turns.py", "--verify", path,
                                         "--dictionary", PILOT / "words.txt"], stderr=subprocess.STDOUT)
            self.assertIn(b"case 1", caught.exception.output)

    def test_distinct_pairs_ignore_rack_order_and_keep_letter_multiplicities(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            dictionary = root / "words.txt"
            dictionary.write_text("EA\nEAT\nEATS\n")
            manifest = root / "games.csv"
            manifest.write_text("game,source\nfirst.csv,one\nsecond.csv,two\n")
            header = "player,rack,square,play\n"
            (root / "first.csv").write_text(header + "A,EAT,8H,EAT\nB,SST,8H,...S\n")
            for rack in ("TSS", "STT"):
                (root / "second.csv").write_text(
                    header + f"A,EA,8H,EA\nB,T,8H,..T\nA,{rack},8H,...S\n")
                run = subprocess.run([sys.executable, "bench/sample_turns.py", "--games", manifest,
                                      "--dictionary", dictionary], text=True, capture_output=True)
                if rack == "TSS":
                    self.assertNotEqual(run.returncode, 0)
                    self.assertIn("distinct", run.stderr)
                else:
                    self.assertEqual(run.returncode, 0, run.stderr)
                    cases = json.loads(run.stdout)["cases"]
                    self.assertEqual(cases[0]["board"], cases[1]["board"])
                    self.assertEqual([c["rack"] for c in cases], ["SST", "STT"])

    def test_reassigns_a_game_when_its_first_choice_blocks_another_stage(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            dictionary = root / "words.txt"
            dictionary.write_text("EA\nEAT\nEATS\nMEATS\n")
            manifest = root / "games.csv"
            manifest.write_text("game,source\na.csv,A\nb.csv,B\n")
            header = "player,rack,square,play\n"
            (root / "a.csv").write_text(header + "A,EATLMNO,8H,EAT\nB,SSTXILU,8H,...S\nA,LMNORSZ,8G,M....\n")
            (root / "b.csv").write_text(header + "A,EASSTXI,8H,EA\nB,TUVWXYZ,8H,..T\nA,SSTXILU,8H,...S\n")
            corpus = json.loads(sample("--games", manifest, "--dictionary", dictionary,
                                       "--size", "2", "--seed", "0"))
            self.assertEqual([(c["game"], c["stage"]) for c in corpus["cases"]],
                             [("a.csv", "middle"), ("b.csv", "early")])

    @unittest.skipUnless(TIMING_DICTIONARY.is_file(), "full timing dictionary is supplied separately")
    def test_reproduces_the_committed_corpus_byte_for_byte(self):
        output = subprocess.check_output([sys.executable, "bench/sample_turns.py", "--seed", "10"])
        self.assertEqual(output, (PILOT / "corpus.json").read_bytes())

    def test_replays_every_saved_case_with_the_fixture_dictionary(self):
        saved = json.loads((PILOT / "corpus.json").read_text())
        settings = ("--seed", str(saved["seed"]))
        first = sample(*settings)
        self.assertEqual(first, sample(*settings))
        corpus = json.loads(first)
        dictionary = PILOT / "words.txt"
        fingerprint = hashlib.sha256(dictionary.read_bytes()).hexdigest()
        self.assertEqual(corpus["stage_counts"], saved["stage_counts"])
        for case, expected in zip(corpus["cases"], saved["cases"], strict=True):
            replay = json.loads(subprocess.check_output([
                sys.executable, "bench/replay_game.py", "--game", PILOT / case["game"],
                "--dictionary", dictionary, "--turn", str(case["turn"])]))
            self.assertEqual((case["board"], case["rack"]), (replay["board"], replay["rack"]))
            self.assertEqual(case["dictionary_sha256"], fingerprint)
            for key in ("game", "source", "turn", "stage", "eligible_rank", "eligible_count",
                        "player", "square", "play", "board", "rack"):
                self.assertEqual(case[key], expected[key], key)

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
