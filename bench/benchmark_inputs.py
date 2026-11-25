"""Read benchmark positions, select their pattern cache and qualify their directional counts."""
import csv
import hashlib
import io
import os
import subprocess
from dataclasses import dataclass
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build"
FIXTURE = ROOT / "bench/fixtures/paired"


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


@dataclass(frozen=True)
class SelectedCache:
    path: Path
    existing: bool


def select_cache(worker, dictionary, requested):
    result = subprocess.check_output([worker, "--select-cache", dictionary, ROOT, requested])
    state, path = result.split(b"\n", 1)
    if state not in (b"existing", b"build"):
        raise ValueError(f"unrecognized pattern cache selection: {state!r}")
    return SelectedCache(Path(os.fsdecode(path)), state == b"existing")


@dataclass(frozen=True)
class Position:
    board: Path
    rack: str

    @classmethod
    def parse(cls, board: Path, rack: str):
        if not rack or len(rack) > 7 or any(c < "A" or c > "Z" for c in rack):
            raise ValueError(f"expected 1-7 uppercase rack tiles, got {rack!r}")
        path = board.resolve(strict=True)
        if any(c in str(path) for c in "\n\r\t"):
            raise ValueError(f"unsupported control character in board path: {path!r}")
        if not path.is_file():
            raise ValueError(f"expected a board file: {path}")
        return cls(path, "".join(sorted(rack)))

    @classmethod
    def from_row(cls, manifest: Path, row, board_column="board", rack_column="rack"):
        try:
            return cls.parse(manifest.parent / row[board_column], row[rack_column])
        except (OSError, ValueError) as error:
            raise ValueError(f"{manifest}: case {row['case']!r}, {board_column}/{rack_column}: {error}") from error


def read_manifest(manifest: Path, extra_columns=()):
    columns = ("case", "stage", "board", "rack", *extra_columns)
    with manifest.open(newline="") as stream:
        reader = csv.DictReader(stream)
        missing = [column for column in columns if column not in (reader.fieldnames or ())]
        if missing:
            raise ValueError(f"{manifest}: missing columns: {', '.join(missing)}")
        rows = list(reader)
    if not rows or len({row["case"] for row in rows}) != len(rows):
        raise ValueError("expected a nonempty manifest with distinct case names")
    for row in rows:
        if any(row[column] is None for column in columns):
            raise ValueError(f"{manifest}: incomplete row for case {row['case']!r}")
    return rows


def qualify_positions(dictionary, shared_cache, positions, variants=(), build=BUILD):
    subprocess.run([build / "perpendicular_checks_test"], check=True)
    positions = dict.fromkeys(positions)
    requests = "".join(f"{pos.board}\t{pos.rack}\n" for pos in positions)
    output = subprocess.check_output(
        [build / "placement_equivalence_test", dictionary, shared_cache.path,
         "--load-cache" if shared_cache.existing else "--build-cache", *variants],
        input=requests, text=True)
    rows = csv.DictReader(io.StringIO(output))
    return {pos: {key: int(value) for key, value in row.items()}
            for pos, row in zip(positions, rows, strict=True)}


def warm_files(paths):
    for path in dict.fromkeys(paths):
        with path.open("rb") as stream:
            while stream.read(1024 * 1024):
                pass
