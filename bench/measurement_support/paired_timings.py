"""Estimate paired solve times and game-level intervals with equal weight per stage."""
import random
from statistics import fmean, quantiles
from typing import NamedTuple

DRAWS = 20000
SEED = 15


class Pair(NamedTuple):
    original: float
    packed: float

    @property
    def saving(self):
        return self.original - self.packed


def stage_mean(groups, field):
    return fmean(fmean(getattr(pair, field) for pair in group) for group in groups)


class Estimate(NamedTuple):
    original: float
    packed: float
    saving: float
    low: float
    high: float


def summarize(groups):
    rng = random.Random(SEED)
    # A difference encodes both observations from the same game. Draw the
    # original number of games in each stage, then equally weight stage means.
    savings = [[pair.saving for pair in group] for group in groups]
    draws = [fmean(fmean(rng.choices(stage, k=len(stage))) for stage in savings)
             for _ in range(DRAWS)]
    percentiles = quantiles(draws, n=1000, method="inclusive")
    return Estimate(stage_mean(groups, "original"), stage_mean(groups, "packed"),
                    stage_mean(groups, "saving"), percentiles[24], percentiles[974])
