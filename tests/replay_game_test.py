"""Replay the saved game through the command used to select benchmark positions."""
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

FIXTURE = Path("bench/fixtures/replay")


def replay(*args):
    return subprocess.check_output([sys.executable, "bench/replay_game.py", *args], text=True)


class ReplayGameTest(unittest.TestCase):
    def test_lists_supported_post_opening_turns_in_record_order(self):
        self.assertEqual(json.loads(replay()), [2, 3, 4, 5, 6, 8, 10, 11, 12, 13, 14, 15, 16, 18, 19, 20])

    def test_blank_rack_is_ineligible_even_when_the_player_keeps_the_blank(self):
        result = subprocess.run([sys.executable, "bench/replay_game.py", "--turn", "7"],
                                capture_output=True, text=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("turn 7 is ineligible", result.stderr)

    def test_dictionary_must_include_words_formed_at_right_angles(self):
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
            for number in json.loads(replay()):
                board = Path(directory) / f"{number}.csv"
                turn = json.loads(replay("--turn", str(number), "--board-csv", board))
                square = turn["square"]
                down = square[0].isalpha()
                row = int(square[1:] if down else square[:-1]) - 1
                col = ord(square[0] if down else square[-1]) - ord("A")
                word = "".join(tile if tile != "." else turn["board"][row + i * down][col + i * (not down)]
                               for i, tile in enumerate(turn["play"])).upper()
                view_row, start = (col, row) if down else (row, col)
                requests.append(f"{board}\t{turn['rack']}\t{int(down)} {view_row} {start} {word}\n")
            result = subprocess.run(["build/placement_equivalence_test", FIXTURE / "words.txt",
                                     Path(directory) / "cache"], input="".join(requests),
                                    text=True, capture_output=True)
            self.assertEqual(result.returncode, 0, result.stderr)


if __name__ == "__main__":
    unittest.main()
