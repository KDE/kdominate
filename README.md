# KDominate

KDominate is a tactical game for one or two players, where players place and convert tiles with the goal of controlling the majority of the board.

Each player starts with some tiles of their own on the board, and they can either clone a tile to an adjacent empty space, or jump with the tile to an empty position 2 spaces away. After cloning or jumping, all the adjacent enemy tiles become your own.

Some boards contain obstacles where tiles can't be placed nor jumped over.

The game ends when the board is full, and the winner is the player who owns more tiles.

There's no "pass" move, if one player can no longer make any move, all the remaining tiles become the other player's and the game ends immediately. 

You can play against a human or against a computer AI, with adjustable difficulty levels.

### Explanation of the code

The code is divided in the following classe:

- KDominateBoard: The internal reprentation of the board, without UI.
- KTileWidget: displays a single tile, with the indicated color and/or effects
- KBoardWidget: Contains an array of KTileWidget and coordinates the animations of effects on them.
- Game: Reads the state of the KDominateBoard and updates the KBoardWidget, and handles the user/AI interaction with the board.
- KDominateAI: Logic for the computer players (minimax with alpha-beta prunning), operates on a KDominateBoard. Computation runs in a background thread.
- KDominate: The game window, menu bar and status bar. Instantiates a the Game and the KBoardWidget.
- SettingsWidget: The preferences window.
- KDominateAiTest: Autotests. They operate only on KDominateAI and KDominateBoard, so they don't touch any UI code.

Some remarks:

- KDominate is the entry point.
- Game contains a state machine so the user/AI interactions have to wait on each other and on animations.
- After a move all the tiles in KDominateBoard get updated immediately, it's KBoardWidget which takes care of animating the changes tile-by-tile for the user.
