"""Build a sampler manifest from the pinned Quackle self-play archive."""
import argparse
import csv
import hashlib
import re
import tarfile
from pathlib import Path

from replay_game import load_dictionary, replay_events

REVISION = "b4c0ccf7c6db50399a3f867cb359b45c861292f0"
ARCHIVE_SHA256 = "e2ec01161abe1fd6a21647914441cb1797784215e4d24e3ac472aac30455beeb"
SOURCE = f"https://github.com/kmcelwee/scrabble/blob/{REVISION}"
PLACEMENT = re.compile(r">([^:]+): ([A-Z?]{1,7}) ([0-9]+[A-O]|[A-O][0-9]+) ([a-zA-Z.]+) \+\d+ \d+")
SETTLEMENT = re.compile(r">[^:]+: +\([A-Z?]+\) \+\d+ \d+")


def placements(data):
    lines = [line for line in data.decode("ascii").splitlines() if line.startswith(">")]
    if not lines or not SETTLEMENT.fullmatch(lines[-1]):
        return []
    events = []
    for line in lines[:-1]:
        match = PLACEMENT.fullmatch(line)
        if not match:
            return []  # Keep complete games representable by the placement-only CSV.
        events.append(dict(zip(("player", "rack", "square", "play"), match.groups())))
    return events


def import_games(archive, directory, dictionary):
    if hashlib.sha256(archive.read_bytes()).hexdigest() != ARCHIVE_SHA256:
        raise ValueError(f"expected Quackle archive SHA-256 {ARCHIVE_SHA256}")
    words, _ = load_dictionary(dictionary)
    records = []
    with tarfile.open(archive, "r|gz") as source:
        for member in source:
            path = Path(member.name).relative_to(f"scrabble-{REVISION}")
            if not member.isfile() or not path.match("data/*/*.gcg"):
                continue
            stream = source.extractfile(member)
            assert stream is not None  # Regular file, as checked above.
            events = placements(stream.read())
            if not events:
                continue
            try:
                turns = replay_events(words, events)
            except ValueError:
                continue  # Every move must be legal under the supplied dictionary.
            if not turns:
                continue
            game = path.with_suffix(".csv")
            dest = directory / game
            dest.parent.mkdir(parents=True, exist_ok=True)
            with dest.open("w", newline="") as stream:
                writer = csv.DictWriter(stream, fieldnames=events[0], lineterminator="\n")
                writer.writeheader()
                writer.writerows(events)
            records.append((game.as_posix(), f"{SOURCE}/{path.as_posix()}"))
    if not records:
        raise ValueError("no complete supported games matched the dictionary")
    manifest = directory / "games.csv"
    with manifest.open("w", newline="") as stream:
        writer = csv.writer(stream, lineterminator="\n")
        writer.writerow(("game", "source"))
        writer.writerows(sorted(records))
    print(f"Wrote {len(records)} replayable games to {manifest}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("archive", type=Path)
    parser.add_argument("directory", type=Path)
    parser.add_argument("--dictionary", type=Path, default=Path("data/dictionaries/sample_dictionary.txt"))
    args = parser.parse_args()
    try:
        import_games(args.archive, args.directory, args.dictionary)
    except (ValueError, OSError) as error:
        parser.error(str(error))
