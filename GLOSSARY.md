# Scrabble solver

The solver finds moves a player can make with their rack after the opening move, when the board already contains tiles.

## Language

**Dictionary**

The set of words valid for the game. Every word already on the board and every word formed by a move belongs to this set.

**Candidate placement**

A proposal to play one or more rack tiles together on specific empty squares, with all words judged on the resulting board. Placing S and C before an existing `AT` to form `SCAT` requires only `SCAT` to be checked along that row.

**Candidate decision**

An acceptance or rejection of a candidate placement for a particular word, starting square and direction. If `CAT` starting at H8 across is considered and rejected twice, that accounts for two candidate decisions.

**Connected placement**

A candidate placement in which at least one newly placed tile shares an edge with a tile already on the board. Corner-only contact leaves the placement disconnected.

**Move**

A legal, connected placement whose new tiles all lie in one row or column, with every square between them occupied by a new or existing tile and every word formed in the dictionary. A move is identified by the letters placed and their squares, so one A that completes both `SEA` across and `TEA` down counts once.

**Word formed by a move**

A complete, uninterrupted run of two or more letters across or down that includes at least one tile placed by the move. Each run counts separately, so a single T that completes `AT` across and `AT` down forms two words.

**Directional count**

The number of accepted candidate decisions for one direction. A single A that completes `SEA` across and `TEA` down contributes one to each directional count and makes one move.

**Normal board**

The board in its playing orientation. Across words read left to right and down words read top to bottom.

**Perpendicular board**

The same position viewed with rows and columns exchanged. A column read top to bottom on the normal board reads left to right in the corresponding row of the perpendicular board.

**Horizontal word window** (window)

A span of at least two consecutive squares in a row of the normal board or the perpendicular board where filling one to seven empty squares could form a complete horizontal word connected to existing tiles. The board determines the window independently of the rack, with an empty square or the board edge just beyond each end.

**Word pattern**

The sequence of existing letters and empty squares read left to right in a window before tiles are played, such as `..` in the two squares directly below `AT`. Two windows covering different squares can share the same pattern, such as `.AT`, even if one is on the normal board and the other is on the perpendicular board.

**Pattern match**

A word matches a word pattern when it has the same length and agrees with every existing letter in its position. For example, `CAT` matches `.AT` even in a window where placing C would form an invalid perpendicular word.

**Rack**

The player's supply of up to seven tiles available for a move. Their order is immaterial, and each copy of a repeated letter is available to play.

**Rack subset**

A selection of tiles from the rack, identified by its letters and how many of each it contains. To form `FEEL` in a window with pattern `F...`, the rack subset must supply two Es and one L.

**Anagram group**

All dictionary words that match a given word pattern and fill its empty squares with exactly the same rack subset. For the pattern `F...` and subset `EEL`, `FEEL` and `FLEE` belong to the same anagram group.

**Perpendicular word**

A word formed through a newly placed tile at right angles to the word being considered. In the `SEA` and `TEA` example, each is perpendicular to the other.

**Preexisting perpendicular prefix word** (prefix word)

The uninterrupted run of existing tiles immediately before the square to be filled, read in the direction of the perpendicular word. In `AT.AS`, placing L forms `ATLAS` with `AT` as the prefix word.

**Preexisting perpendicular suffix word** (suffix word)

The uninterrupted run of existing tiles immediately after the square to be filled, read in the direction of the perpendicular word. In `AT.AS`, the suffix word is `AS`. Either run can be a single tile.

**Perpendicular-check set**

The letters allowed at an empty square by the perpendicular word requirement alone, in the board view being considered. The set contains every letter when no perpendicular word would form, and only `{B, M}` when `BA` and `MA` are the only dictionary words that fit the perpendicular pattern `.A`.
