# The Lost Vault

A console-based 2D treasure hunt game written in C++17. Explore a 10 × 10 grid, collect five relics, avoid traps, and find your way to the exit.

## How to Play

Collect all five relics and reach the exit (`E`) to win. Each relic is worth 10, 15, or 25 points.

You start with 3 health points. Every time you step on a trap (`^`), you lose one health point. Losing all your health ends the game.

Reaching the exit before collecting all the relics won't unlock it. You can quit at any time or start a new round after a game ends.

## Controls

| Key | Action |
|---|---|
| `W` | Move up |
| `A` | Move left |
| `S` | Move down |
| `D` | Move right |
| `Q` | Quit |

Enter a key and press **Enter** to make a move. Uppercase and lowercase inputs are both supported.

## Map Legend

| Symbol | Meaning |
|---|---|
| `P` | Player |
| `T` | Uncollected relic |
| `^` | Trap |
| `#` | Wall |
| `E` | Exit |
| `.` | Empty passage |

## Features

- Fixed, hand-designed 10 × 10 map
- Five named relics with different point values
- Health and traps
- Score, relic count, and move counter
- Movement and wall collision checks
- Victory, defeat, and replay
- No external libraries required

## Requirements

- A C++17-compatible compiler
- `g++` (GCC or MinGW-w64) for the commands below

## Build and Run

Clone the repository and open a terminal in the project directory.

### Windows (MinGW-w64)

Compile:

```powershell
g++ -std=c++17 -Wall -Wextra -pedantic main.cpp -o lost_vault.exe
```

Run:

```powershell
.\lost_vault.exe
```

### Linux

Compile:

```bash
g++ -std=c++17 -Wall -Wextra -pedantic main.cpp -o lost_vault
```

Run:

```bash
./lost_vault
```

## How It Works

The game is organised around a `Game` class that manages the map, player position, relics, health, score, and current game state.

The map is stored separately from the player's position, so moving doesn't overwrite the underlying tiles. Each move is checked against the grid boundaries and walls before the player's position and move counter are updated.

When a relic is collected, its points are added to the score and its tile becomes an empty passage. The game continues until the player wins, loses all health, or quits.

## Implementation Notes

- The map uses a fixed array of strings.
- Relic details are stored in a vector.
- Separate functions handle input, movement, treasure collection, display, and game endings.
- Only successful movements count toward the move total.

## Limitations

- The game uses one fixed map.
- The entire map is visible from the start.
- Input is turn-based; it doesn't support real-time movement.
- Progress isn't saved between sessions.

## Screenshots

Add screenshots of the game to the `screenshots/` directory.

Suggested captures:

1. The starting map and status display.
2. Collecting a relic and updating the score.
3. The victory or defeat screen showing the final statistics.
