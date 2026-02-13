# Splendor Ref - AI Engine & Optimizer

This project provides a Splendor game engine, multiple MCTS-based AI engines, and a Genetic Algorithm optimizer for tuning AI performance.

## Compilation

Buil all executables using the provided Makefile into the `build/` directory:

```bash
make all
```

This will create the following binaries in `build/`:
- `referee`: The main game controller.
- `mcts_engine_04`: The primary AI engine (accepts parameters).
- `mcts_engine_01`, `mcts_engine_03`: Baseline MCTS engines.
- `random_engine`: Simple baseline engine.
- `replay`: Replays games from log files.


## Usage Instructions

### Running a Game (Referee)
The referee manages turns and validates moves between two AI players.

```bash
./build/referee [seed]
```
Wait for communication via stdin/stdout after starting.

### MCTS Engine 04 (Optimized Engine)
The primary AI engine. It can be initialized with custom weight parameters for its evaluation heuristic.

```bash
./build/mcts_engine_04 [W_CARD] [W_GEM] [W_JOKER] [W_POINT] [W_NOBLE] [W_NOBLE_PROGRESS] [W_RESERVED_PROGRESS] [W_RESERVED_EFFICIENCY] [W_UNRESERVED_SLOT] [W_BOUGHT_EFFICIENCY]
```
Example:
```bash
./build/mcts_engine_04 0.4 0.25 0.3 20.13 20.7 0.1 0.1 1.06 0.5 1.0
```

### Genetic Algorithm Optimization
Optimize AI evaluation weights using competitive evolution.

```bash
python3 ga_optimizer.py
```
This script runs a population of engines against stronger baselines and evolves the best-performing weights.

### Tournament & Performance Testing
Run a series of games between specific engines to measure win rates.

```bash
python3 tournament_runner.py
```

### Running Tests
Execute the unit test suite to verify game logic.

```bash
make test
```
Or manually run:
```bash
g++ -std=c++11 -Wall -Isrc -o build/test_referee tests/tests.cpp src/game_logic.cpp
./build/test_referee
```

## Cleaning Up
Remove all compiled binaries and object files:

```bash
make clean
```
