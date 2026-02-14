# Splendor Ref - AI Engine & Weight Tuning

This project provides a Splendor game engine, a canonical MCTS v01 AI engine, and an SPSA-based optimizer for tuning evaluation weights.

## Compilation

Build all executables using the provided Makefile into the `build/` directory:

```bash
make all
```

This creates:
- `referee`: Main game controller.
- `mcts_engine_01`: Canonical MCTS + linear-eval engine.
- `random_engine`: Baseline random engine.
- `replay`: Replays games from log files.

## Usage

### Running a Game (Referee)

```bash
./build/referee [seed]
```

### MCTS Engine 01

`mcts_engine_01` uses iteration-capped MCTS with leaf linear evaluation.

```bash
./build/mcts_engine_01 [--sims N] [--seed N] [--max-depth N] [--risk-lambda X] [--det N] [--c-puct X] [weights...]
```

Default options:
- `--sims 3000`
- `--max-depth 18`
- `--det 8`
- `--c-puct 1.25`
- `--risk-lambda 0.30`

Optional positional weights (fixed order for tuning scripts):
1. `W_POINT_SELF`
2. `W_POINT_OPP`
3. `W_GEM_SELF`
4. `W_GEM_OPP`
5. `W_BONUS_SELF`
6. `W_BONUS_OPP`
7. `W_RESERVED_SELF`
8. `W_RESERVED_OPP`
9. `W_NOBLE_PROGRESS_SELF`
10. `W_NOBLE_PROGRESS_OPP`
11. `W_AFFORDABLE_SELF`
12. `W_AFFORDABLE_OPP`
13. `W_WIN_BONUS`
14. `W_LOSS_PENALTY`
15. `W_TURN_PENALTY`
16. `W_EFFICIENCY`
17. `W_DIRECTIONAL_COMMITMENT`

Example:

```bash
./build/mcts_engine_01 --sims 3000 20 20 0.25 0.25 1.2 1.2 0.6 0.6 0.9 0.9 0.8 0.8 1000 1000 0.01 1.0 1.0
```

### Weight Optimization (SPSA)

```bash
python3 scripts/weight_optimizer.py
```

### Tournament / Match Running

```bash
python3 scripts/tournament_runner.py ./build/mcts_engine_01 ./build/random_engine
```

### Running Tests

```bash
make test
```

Or manually:

```bash
g++ -std=c++11 -Wall -Isrc -o build/test_referee tests/tests.cpp src/game_logic.cpp
./build/test_referee
```

## Cleaning Up

```bash
make clean
```
