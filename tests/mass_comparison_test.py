"""Worked examples for the game-level precision and break-even decisions."""
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "bench"))
from mass_comparison import Pair, games_per_stage, summarize, recovery


class MassComparisonTest(unittest.TestCase):
    def test_pilot_sizes_for_each_cache_condition_in_ms(self):
        # Each stage has variance 2. The target half-width is 1 ms on hits
        # and 2 ms on misses. Keep at least two games for the order crossover.
        for original, expected in [(100, 3), (200, 2)]:
            groups = [[Pair(original, original - saving) for saving in stage]
                      for stage in [(1, 3), (5, 7), (9, 11)]]
            self.assertEqual(games_per_stage(groups), expected)

    def test_resample_whole_pairs_and_weight_stages_equally(self):
        # Original times vary within stages. Paired savings are constant.
        groups = [[Pair(x, x - saving) for x in range(100, 100 + count)]
                  for count, saving in [(2, -10), (3, 1), (4, 3)]]
        result = summarize(groups)
        self.assertEqual(result.saving, -2)
        self.assertEqual((result.low, result.high), (-2, -2))
        self.assertEqual(result.original, 101)
        self.assertEqual(result.packed, 103)

    def test_round_up_only_for_resolved_positive_reuse_savings(self):
        self.assertEqual(recovery(-10, 4, (1, 6)), "3")
        self.assertEqual(recovery(-10, 4, (0, 6)), "unresolved")
        self.assertEqual(recovery(-10, -4, (-6, -1)), "no recovery")
        self.assertEqual(recovery(1, 4, (1, 6)), "0")


if __name__ == "__main__":
    unittest.main()
