"""Check the memory report on the qualified two-direction crossing fixture."""
import csv
import io
import subprocess
import tempfile
import unittest
from pathlib import Path
from string import ascii_uppercase

FIXTURE = Path("bench/fixtures/paired")


def report(dictionary, *cache):
    output = subprocess.check_output([
        "python3", "bench/memory_benchmark.py", "--dictionary", dictionary,
        "--board", FIXTURE / "crossings.csv", "--rack", "RT", *cache,
    ], text=True, stderr=subprocess.PIPE)
    return {row["metric"]: row for row in csv.DictReader(io.StringIO(output))}


class MemoryBenchmarkTest(unittest.TestCase):
    def test_first_solve_and_owned_bytes(self):
        rows = report(FIXTURE / "dictionary.txt")
        for row in rows.values():
            self.assertEqual((row["cache"], row["across"], row["down"]), ("miss", "2", "1"))
            self.assertEqual(int(row["packed_bytes"]) - int(row["original_bytes"]),
                             int(row["packed_minus_original_bytes"]))

        def size(metric, variant):
            return int(rows[metric][variant + "_bytes"])

        for variant in ("original", "packed"):
            self.assertGreater(size("dictionary_peak_rss", variant), 0)
            self.assertGreaterEqual(size("first_solve_peak_rss", variant),
                                    size("dictionary_peak_rss", variant))
            self.assertEqual(size("retained_dictionary", variant),
                             size("retained_shared_dictionary", variant) +
                             size("retained_validator", variant))

        # x86-64: seven 752-byte row entries and a 16-byte table.
        self.assertEqual(size("retained_validator", "original"), 5280)
        self.assertEqual(size("validator_construction_peak", "original"), 5344)
        self.assertEqual(rows["retained_shared_dictionary"]["packed_minus_original_bytes"], "0")
        # Each of two views owns prefix and suffix IDs for all 225 squares.
        self.assertEqual(int(rows["retained_board"]["packed_minus_original_bytes"]),
                         4 * 225 * 4 + 48)

    def test_valid_cache_is_loaded_and_invalid_cache_is_preserved(self):
        dictionary = FIXTURE / "dictionary.txt"
        with tempfile.TemporaryDirectory() as directory:
            cache = Path(directory) / "patterns"
            subprocess.run(["build/memory_worker", "original", "miss", dictionary, cache,
                            FIXTURE / "crossings.csv", "RT"], stdout=subprocess.DEVNULL, check=True)
            self.assertEqual(report(dictionary, "--cache", cache)["retained_board"]["cache"], "hit")
            equivalent = Path(directory) / "equivalent"
            equivalent.write_bytes(dictionary.read_bytes().lower().replace(b"\n", b"\r\n") + b"as\r\n")
            probe = subprocess.check_output(
                ["build/memory_worker", "--check-cache", equivalent, cache], text=True)
            self.assertEqual(probe.strip(), "match")
            cache.write_bytes(b"invalid")
            with self.assertRaises(subprocess.CalledProcessError) as failure:
                report(dictionary, "--cache", cache)
            self.assertIn("expected starting condition", failure.exception.stderr)
            self.assertEqual(cache.read_bytes(), b"invalid")

    def test_rehash_peak_and_spare_dictionary_capacity(self):
        with tempfile.TemporaryDirectory() as directory:
            dictionary = Path(directory) / "words"
            # Thirty distinct end letters/roles across two-letter words force a
            # pair-map rehash close to the end of construction on libstdc++.
            words = (FIXTURE / "dictionary.txt").read_text() + "".join(
                f"{letter}{letter}\n" for letter in ascii_uppercase[:15])
            dictionary.write_text(words)
            rows = report(dictionary)
            self.assertGreater(int(rows["validator_construction_peak"]["packed_bytes"]),
                               int(rows["retained_validator"]["packed_bytes"]))
            # Duplicates reserve input pointers and hash buckets, but add no words.
            dictionary.write_text(words + "AS\n" * 1000)
            duplicate = report(dictionary)
            for variant in ("original_bytes", "packed_bytes"):
                self.assertEqual(duplicate["retained_validator"][variant], rows["retained_validator"][variant])
                self.assertGreaterEqual(int(duplicate["retained_shared_dictionary"][variant]) -
                                        int(rows["retained_shared_dictionary"][variant]), 1000 * 8)


if __name__ == "__main__":
    unittest.main()
