# Concurrent Pac-Man

A classic Pac-Man clone written in C with OpenGL (GLUT) rendering, built as an exercise in **operating systems concurrency** — every ghost is its own POSIX thread, and mutexes/semaphores coordinate shared game state instead of a single-threaded game loop.

![Gameplay](running%201.png)

## Why it's interesting

Most student Pac-Man clones run everything on one thread. This one models the game as a small concurrent system:

- **Game engine thread** — advances Pac-Man, resolves pellet/power-pellet pickups, checks the win condition.
- **4 independent ghost threads** — each ghost has its own thread running AI (chase Pac-Man normally, flee when a power pellet is active).
- **Mutexes** guard shared state: the board (`boardMutex`), power-pellet mode (`powerPelletMutex`), and speed-boost allocation (`speedBoostMutex`).
- **Semaphores** model scarce resources: only a limited number of "keys" and "permits" let ghosts leave the ghost house at once (`ghostHouseKeys`, `ghostHousePermits`), and only 2 "speed boost" tokens exist for ghosts to contend over (`speedBoosts`).
- **GLUT timer callback** drives rendering independently of the simulation threads.

## Gameplay

- Eat pellets (+10) and power pellets (+50) to clear the board.
- Power pellets turn ghosts blue and vulnerable — eat them for stacking bonus points (200, 400, 800, 1600).
- Losing all 3 lives ends the game; clearing every pellet wins it.

## Controls

| Key | Action |
|---|---|
| Arrow Up / Right / Down / Left | Move Pac-Man |

## Build & run

Requires a C compiler, `pthread`, and FreeGLUT/OpenGL development libraries.

**Linux / WSL:**
```bash
sudo apt install freeglut3-dev
gcc game.c -o pacman -lglut -lGL -lGLU -lpthread
./pacman
```

**macOS:**
```bash
brew install freeglut
gcc game.c -o pacman -framework OpenGL -framework GLUT -lpthread
./pacman
```

## Screenshots

| | |
|---|---|
| ![1](running%201.png) | ![2](running%202.jpg) |
| ![3](running%203.png) | ![4](running%204.jpg) |

## Background

Originally built as an Operating Systems course project (thread synchronization: mutexes, semaphores, producer/consumer-style resource contention) using a Pac-Man game as the demo application.
