# KDominate

KDominate is a tactical game for one or two players, where players place and convert tiles with the goal of controlling the majority of the board.

Each player starts with some tiles of their own on the board, and they can either clone a tile to an adjacent empty space, or jump with the tile to an empty position 2 spaces away. After cloning or jumping, all the adjacent enemy tiles become your own.

Some boards contain obstacles where tiles can't be placed nor jumped over.

The game ends when the board is full, and the winner is the player who owns more tiles.

There's no "pass" move, if one player can no longer make any move, all the remaining tiles become the other player's and the game ends immediately. 

You can play against a human or against a computer AI, with adjustable difficulty levels.

<img src="https://invent.kde.org/websites/product-screenshots/-/raw/b053896c0bf5d4a102a3af6cf73cdd9de758b0e4/kdominate/kdominate.png?inline=false"/>

## Code layout

The code is divided in the following classes:

### KDominateBoard

The internal reprentation of the board state and the logic to move tiles on it.

### KTileWidget

Widget that displays a single tile, with the indicated color and/or effects

### KBoardWidget

Widget that contains an array of KTileWidget and coordinates the animations and effects on them. After KDominateBoard is updated by a move, KBoardWidget visually animates the changes tile-by-tile.

### Game

Reads the state of the KDominateBoard and updates the KBoardWidget when it changes, and executes the user/AI moves on the KDominateBoard. Contains a state machine so the user/AI interactions wait on each other and on animations.

### KDominateAI

Logic for the computer players (minimax with alpha-beta prunning). Computation runs in a background thread on a copy of KDominateBoard.

### KDominate (entrypoint)

The game window, with a menu bar and status bar. Instantiates a the Game and the KBoardWidget. It is the entry point after main.cpp.

### SettingsWidget

The preferences window.

### KDominateAiTest

Autotests. They interact only with KDominateAI and KDominateBoard, so they don't run any UI code. It runs the AI on map files with board states that reproduce edge cases/bugs I encountered during testing.
