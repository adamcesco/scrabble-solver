"""Check the statistics and the printed board through the public CLI."""
import re
import subprocess
import tempfile
import unittest
from pathlib import Path


class SolverCliTest(unittest.TestCase):
    def test_reports_both_directions_and_restores_the_board(self):
        binary = Path("build/main").resolve()
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "data/caches").mkdir(parents=True)
            rows = ["C" + "." * 14] + ["." * 15] * 14
            (root / "board.csv").write_text("".join(",".join(row) + "\n" for row in rows))
            (root / "words.txt").write_text("CA\n")
            # The built-in rack supplies A to the right of C or below it.
            output = subprocess.check_output([binary, "board.csv", "words.txt"], cwd=root, text=True)
        before, after = re.split(r"(?m)^Moves: 2\nSeconds: [0-9]+\.[0-9]{6}\n\n", output)
        self.assertEqual(before, after)
        self.assertEqual(len(re.findall(r"(?m)^row\s+\d+ ", before)), 15)


if __name__ == "__main__":
    unittest.main()
