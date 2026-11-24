"""List eligible turns or recover a player's position before a recorded move."""
import argparse
import csv
import hashlib
import json
import re
from collections import Counter
from pathlib import Path

FIXTURE = Path(__file__).resolve().parent / "fixtures/replay"
GAME = FIXTURE / "sjoholm-del-solar.csv"
SIZE = 15


def play_squares(square, length):
    across = square[0].isdigit()
    number, column = (square[:-1], square[-1]) if across else (square[1:], square[0])
    row, col = int(number) - 1, ord(column) - ord("A")
    return [(row if across else row + i, col + i if across else col) for i in range(length)]


def place(board, event):
    rack, play = event["rack"], event["play"]
    if not re.fullmatch(r"[A-Z?]{1,7}", rack) or not re.fullmatch(r"[A-Za-z.]+", play):
        raise ValueError("invalid rack or play")
    squares = play_squares(event["square"], len(play))
    placed = {}
    for (row, col), tile in zip(squares, play):
        if not (0 <= row < SIZE and 0 <= col < SIZE):
            raise ValueError("play extends off the board")
        if ((row, col) in board) != (tile == "."):
            raise ValueError("play disagrees with board occupancy")
        if tile != ".":
            placed[row, col] = tile
    needed = Counter("?" if tile.islower() else tile for tile in placed.values())
    if not placed or needed - Counter(rack):
        raise ValueError("move must place tiles from the recorded rack")
    if not board:
        if (SIZE // 2, SIZE // 2) not in placed or len(placed) < 2:
            raise ValueError("opening word must cover the center")
    elif not any(neighbor in board for r, c in placed
                 for neighbor in ((r - 1, c), (r + 1, c), (r, c - 1), (r, c + 1))):
        raise ValueError("disconnected move")
    board.update(placed)


def replay(words, game=GAME):
    board = {}
    rows = ["." * SIZE] * SIZE
    eligible = {}
    with game.open(newline="") as stream:
        for turn, event in enumerate(csv.DictReader(stream), 1):
            if turn > 1 and "?" not in event["rack"]:
                eligible[turn] = dict(turn=turn, **event, board=rows)
            try:
                place(board, event)
                rows = ["".join(board.get((r, c), ".") for c in range(SIZE)) for r in range(SIZE)]
                for line in (*rows, *zip(*rows)):
                    for word in re.findall(r"[A-Z]{2,}", "".join(line).upper()):
                        if word not in words:
                            raise ValueError(f"word {word} is absent from the dictionary")
            except ValueError as error:
                raise ValueError(f"turn {turn}: {error}") from error
    return eligible


def load_dictionary(path):
    data = path.read_bytes()
    return set(data.decode("ascii").upper().splitlines()), hashlib.sha256(data).hexdigest()


def write_board_csv(path, rows):
    with path.open("w", newline="") as stream:
        csv.writer(stream, lineterminator="\n").writerows(row.upper() for row in rows)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--game", type=Path, default=GAME, help="CSV of recorded racks and plays")
    parser.add_argument("--turn", type=int, help="one-based turn number (default: list eligible turns)")
    parser.add_argument("--dictionary", type=Path, default=FIXTURE / "words.txt")
    parser.add_argument("--board-csv", type=Path, help="write the selected board in solver CSV format")
    args = parser.parse_args()
    if args.board_csv and args.turn is None:
        parser.error("--board-csv requires --turn")
    try:
        words, fingerprint = load_dictionary(args.dictionary)
        eligible = replay(words, args.game)
    except ValueError as error:
        parser.error(str(error))
    if args.turn is None:
        print(json.dumps(list(eligible)))
    elif args.turn not in eligible:
        parser.error(f"turn {args.turn} is ineligible (eligible turns: {list(eligible)})")
    else:
        position = dict(game=args.game.name, dictionary_sha256=fingerprint,
                        **eligible[args.turn])
        if args.board_csv:
            write_board_csv(args.board_csv, position["board"])
        print(json.dumps(position, indent=2))


if __name__ == "__main__":
    main()
