"""Regression checks for comparing independently captured candidate decisions."""

from collections import Counter
import tempfile
import unittest
from pathlib import Path

from .qualification import compare_direction, read_decisions


class DecisionQualificationTest(unittest.TestCase):
    def test_missing_duplicate_identifies_decision_despite_equal_totals(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "trace.csv"
            header = "direction,row,column,word,accepted\n"
            first = "down,3,5,FEEL,1\n"
            second = "down,3,5,FLEE,1\n"
            path.write_text(header + first + first + second)
            reference = read_decisions(path)
            path.write_text(header + second + first + second)
            production = read_decisions(path)

        with self.assertRaisesRegex(ValueError, r"game1 packed down.*missing 1.*\(3,5\).*FEEL.*accepted"):
            compare_direction("game1 packed down", reference["down"], production["down"], 3, 3)

    def test_solver_must_return_the_accepted_count_in_each_direction(self):
        decisions = Counter({(3, 5, "FEEL", True): 2, (3, 5, "FLEE", False): 1})
        with self.assertRaisesRegex(ValueError, r"game2 rack-neighbor across.*returned 3.*accepted 2"):
            compare_direction("game2 rack-neighbor across", decisions, decisions, 2, 3)


if __name__ == "__main__":
    unittest.main()
