# Rocks'n'Diamonds with The Puzzle Pits

[Rocks'n'Diamonds](https://www.artsoft.org/rocksndiamonds/) 4.4.2.4, by Holger Schemel, with a new game engine that plays [The Puzzle Pits](https://github.com/LaPingvino/The-Puzzle-Pits) (Abe Pralle, Plasma Works, 1995): its own level files, by its own rules, with its own pictures and sounds. R'n'D already plays Supaplex, Emerald Mine, Boulder Dash and Mirror Magic this way; The Puzzle Pits joins them.

This is a proposal to Rocks'n'Diamonds, in the open: R'n'D isn't developed on GitHub, so the change is also here as a patch series for its author.

## Playing

```sh
make
./rocksndiamonds
```

Choose the level set *The Puzzle Pits* (it is in `levels/The_Puzzle_Pits`). A level set of your own is a folder with the levels (`LEVEL01.PIT`, `LEVEL02.PIT` ...), the game's `TILES1.DAT` to `TILES3.DAT` and its sounds, and a `levelinfo.conf` with `filename: LEVEL%02d.PIT` and `filetype: PITS`. Levels made with [the Puzzle Pits level editor](https://lapingvino.github.io/The-Puzzle-Pits/editor/) work as they are.

| | |
|---|---|
| Directions | walk, push (hold against a box) |
| Snap + left, up, right, down | use the red, green, blue, yellow gem |
| Drop + left, up, right | use the red, green, blue key |
| Snap | wait a turn |
| Mouse | left button: step towards a square; right button: walk there |

A script's text shows in R'n'D's envelopes. When a script asks a question, a room of letter keys opens: walk to a letter and snap to type it, then walk onto OK. When it asks for a square, move the frame and snap.

## What changed in R'n'D

- `src/game_pits/`: the engine.
- Hooks, as for the other engines: `src/main.h` (`LEVEL_FILE_TYPE_PITS`, `GAME_ENGINE_TYPE_PITS`), `src/files.c` (loading `.pit` files and level sets with `filetype: PITS`; R'n'D's own copy of a level, for its preview and editor), `src/game.c` (start, a frame, the end; the panel; text in envelopes), `src/tools.c` (drawing), `src/editor.c` and `src/screens.c` (the engine's name), the Makefiles.
- `levels/The_Puzzle_Pits`: the game's 40 levels, pictures and sounds.

The same as patches against R'n'D 4.4.2.4, in [`patches/`](patches) (`git am patches/*.patch`):

1. The engine's place in R'n'D: the hooks and its interface.
2. The engine.
3. The level set.

## How it was made

The Puzzle Pits is open source under the Apache License 2.0, which can't be combined with R'n'D's GPL version 2. So the engine is a new implementation, written only from a description of the game: [PITS-RULES.md](https://github.com/LaPingvino/The-Puzzle-Pits/blob/master/docs/PITS-RULES.md) (how it plays and is drawn) and [PIT-FORMAT.md](https://github.com/LaPingvino/The-Puzzle-Pits/blob/master/docs/PIT-FORMAT.md) (its files and script language), by someone who hadn't seen the game's code. It was then played against the original's rules turn by turn, on random moves in all 40 levels, and the description corrected where they differed; 34 of the levels play identically over 1500 moves each, the others differ only in how fast lava cools and in timing within a turn.

The level set's files (the levels, pictures and sounds) are The Puzzle Pits' own, under the Apache License 2.0 (`levels/The_Puzzle_Pits/LICENSE`). The engine is offered under Rocks'n'Diamonds' own licence.

Made by Joop Kiefte, with Claude (Anthropic).
