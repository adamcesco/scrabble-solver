"""Regression checks for comparing independently captured candidate decisions."""

from collections import Counter
import tempfile
import unittest
from pathlib import Path

from bench.measurement_support.paired_timings import Pair, summarize
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

        with self.assertRaisesRegex(ValueError, r"game1 down.*missing 1.*\(3,5\).*FEEL.*accepted"):
            compare_direction("game1 down", reference["down"], production["down"], 3, 3)

    def test_solver_must_return_the_accepted_count_in_each_direction(self):
        decisions = Counter({(3, 5, "FEEL", True): 2, (3, 5, "FLEE", False): 1})
        with self.assertRaisesRegex(ValueError, r"game2 across.*returned 3.*accepted 2"):
            compare_direction("game2 across", decisions, decisions, 2, 3)


class PairedTimingTest(unittest.TestCase):
    def test_resample_whole_pairs_and_weight_stages_equally(self):
        # Original times vary within stages. Paired savings are constant.
        groups = [[Pair(x, x - saving) for x in range(100, 100 + count)]
                  for count, saving in [(2, -10), (3, 1), (4, 3)]]
        result = summarize(groups)
        self.assertEqual(result.saving, -2)
        self.assertEqual((result.low, result.high), (-2, -2))
        self.assertEqual(result.baseline, 101)
        self.assertEqual(result.production, 103)


if __name__ == "__main__":
    unittest.main()
