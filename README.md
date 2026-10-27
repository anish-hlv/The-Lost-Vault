# The Lost Vault

**The Lost Vault** is a console-based 2D treasure-hunt game written in
C++17. Explore a randomly generated 10×10 grid, collect five relics,
avoid traps, and reach the exit before the three-minute countdown
expires.

## Objective

Collect all five relics and reach the exit (`E`) before time runs out.
Each relic has a different point value. You begin with three health
points, and stepping on a trap reduces your health. The game ends when
you win, lose all health, run out of time, or quit.

## Controls

  Key   Action
  ----- ------------------------
  `W`   Move up
  `A`   Move left
  `S`   Move down
  `D`   Move right
  `Q`   Quit the current round

Enter a key and press **Enter** to move. Uppercase and lowercase
movement commands are supported.

## Map Legend

  Symbol   Meaning
  -------- -------------------
  `P`      Player
  `T`      Uncollected relic
  `^`      Trap
  `#`      Wall
  `E`      Exit
  `.`      Empty passage

## Features

-   Randomly generated 10×10 maps.
-   Five named relics with different point values.
-   Three-minute real-time countdown per round.
-   Three health points and trap hazards.
-   Score, relic collection, health, and move counters.
-   Collision checks for walls and grid boundaries.
-   Map validation to check that relics and the exit are reachable,
    including a health-aware search for a survivable route.
-   Win, defeat, quit, and replay flows.
-   A new map and reset round statistics when replaying.

## Map Generation

The game uses a seeded `std::mt19937` random engine to shuffle candidate
interior cells. Each generated map places 14 interior walls, five
relics, and four traps, while protecting the starting cell and exit. The
game validates the map and retries generation when the map fails its
checks. A connected fallback map is used if the retry limit is reached.

## Timer

Each round has a three-minute time limit. The remaining time is shown in
`MM:SS` format and is based on elapsed real time. Interactive timed
input is implemented separately for Windows and Linux so the countdown
can update while the player waits or types a command. The Linux-specific
input branch uses terminal facilities and should be tested on Linux
before relying on it there.

When input is redirected from a file or pipe rather than entered
interactively, the program reads a complete line; the countdown cannot
refresh or interrupt a blocked line read in that mode.

## Requirements

-   A C++17-compatible compiler.
-   `g++` (GCC or MinGW-w64 on Windows).

No third-party libraries are required.

## Build and Run

Open a terminal in the project directory.

### Windows (PowerShell)

``` powershell
g++ -std=c++17 -Wall -Wextra -pedantic main.cpp -o lost_vault.exe
.\lost_vault.exe
```

### Linux

``` bash
g++ -std=c++17 -Wall -Wextra -pedantic main.cpp -o lost_vault
./lost_vault
```

## Code Structure

-   `Game` manages the game state and round lifecycle.
-   `initializeGame()` resets the round and requests a new map.
-   `generateMap()` creates and validates randomized maps.
-   `mapIsSolvable()` checks connectivity of the start, relics, and
    exit.
-   `mapIsPlayable()` searches game states that include relic
    collection, health, and trap encounters to check for a survivable
    winning route.
-   `playRound()` runs the main gameplay loop and enforces the deadline.
-   `readTimedLine()` handles interactive timed input.
-   `movePlayer()` validates movement and processes traps, relics, and
    the exit.

## Limitations

-   The game uses a fixed 10×10 grid and one set of game rules.
-   The map is displayed in full, so exploration is not hidden by fog of
    war.
-   The timer's interactive input behavior is platform-specific.
-   Linux timed input has not been verified in the current Windows test
    environment.

## Screenshots

Add genuine screenshots of your own gameplay to a `screenshots/` folder
before submission. Suggested screenshots:

-   The starting grid and status panel.
-   A relic collection message.
-   A victory or defeat screen.
