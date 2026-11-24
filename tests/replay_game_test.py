"""Replay the saved game through the command used to select benchmark positions."""
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

FIXTURE = Path("bench/fixtures/replay")
ACROSS, DOWN = 0, 1
# Complete words at their zero-based row and start in the solver's directional view.
RECORDED_PLAYS = {
    2: (DOWN, 7, 3, "LIGATURE"),
    3: (ACROSS, 11, 7, "STIBINE"),
    4: (DOWN, 14, 7, "KOANS"),
    5: (DOWN, 13, 9, "ZEE"),
    6: (DOWN, 8, 6, "WYN"),
    8: (ACROSS, 4, 3, "ROMAINES"),
    10: (ACROSS, 3, 0, "TAUON"),
    11: (DOWN, 0, 3, "TEREBIC"),
    12: (DOWN, 12, 2, "JUNIOR"),
    13: (ACROSS, 5, 9, "HO"),
    14: (ACROSS, 3, 11, "QUA"),
    15: (DOWN, 14, 0, "LOAD"),
    16: (ACROSS, 13, 5, "DECALOGS"),
    18: (DOWN, 2, 9, "WAFFIE"),
    19: (ACROSS, 14, 12, "IVY"),
    20: (ACROSS, 10, 1, "PAIR"),
}


def replay(*args):
    return subprocess.check_output([sys.executable, "bench/replay_game.py", *args], text=True)


class ReplayGameTest(unittest.TestCase):
    def test_can_replay_a_different_recorded_game(self):
        pilot = FIXTURE.parent / "pilot"
        position = json.loads(replay("--game", pilot / "games/6zTBhes669.csv",
                                     "--dictionary", pilot / "words.txt", "--turn", "2"))
        self.assertEqual(position["game"], "6zTBhes669.csv")
        self.assertEqual(position["rack"], "AAEKNST")
        self.assertEqual(position["board"][7], "...CHILI.......")

    def test_lists_supported_post_opening_turns_in_record_order(self):
        self.assertEqual(json.loads(replay()), list(RECORDED_PLAYS))

    def test_blank_rack_is_ineligible_even_when_the_player_keeps_the_blank(self):
        result = subprocess.run([sys.executable, "bench/replay_game.py", "--turn", "7"],
                                capture_output=True, text=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("turn 7 is ineligible", result.stderr)

    def test_dictionary_must_include_perpendicular_words(self):
        with tempfile.TemporaryDirectory() as directory:
            dictionary = Path(directory) / "words.txt"
            words = (FIXTURE / "words.txt").read_text().splitlines()
            words.remove("MIGHTY")  # The Y in WYN extends the opening word.
            dictionary.write_text("\n".join(words))
            result = subprocess.run([sys.executable, "bench/replay_game.py", "--dictionary", dictionary],
                                    capture_output=True, text=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("turn 6: word MIGHTY is absent", result.stderr)

    def test_replays_turn_18_before_cesar_places_waffi(self):
        saved = (FIXTURE / "turn-18.json").read_text()
        first = replay("--turn", "18")
        self.assertEqual(first, saved)
        self.assertEqual(replay("--turn", "18"), first)
        before = json.loads(first)
        self.assertEqual((before["player"], before["rack"]), ("Cesar", "AFFIIPW"))
        after = json.loads(replay("--turn", "19"))
        changes = [(row, col, old, new)
                   for row, (a, b) in enumerate(zip(before["board"], after["board"], strict=True), 1)
                   for col, (old, new) in enumerate(zip(a, b, strict=True), 1) if old != new]
        self.assertEqual(changes, [(row, 3, ".", tile) for row, tile in enumerate("WAFFI", 10)])

    def test_solver_accepts_the_recorded_plays_on_recovered_positions(self):
        requests = []
        with tempfile.TemporaryDirectory() as directory:
            for number, (down, row, start, word) in RECORDED_PLAYS.items():
                board = Path(directory) / f"{number}.csv"
                turn = json.loads(replay("--turn", str(number), "--board-csv", board))
                # Change the order while preserving every tile in the recorded rack.
                rack = turn["rack"][::-1]
                requests.append(f"{board}\t{rack}\t{down} {row} {start} {word}\n")
            result = subprocess.run(["build/placement_equivalence_test", FIXTURE / "words.txt",
                                     Path(directory) / "cache"], input="".join(requests),
                                    text=True, capture_output=True)
            self.assertEqual(result.returncode, 0, result.stderr)


if __name__ == "__main__":
    unittest.main()
