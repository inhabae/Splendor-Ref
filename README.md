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
./build/referee [seed] [--no-log]
```

Referee logging controls:
- `--no-log`: Disable writing `game.log`.
- `--log`: Enable writing `game.log` (default).
- `--log-file PATH`: Write referee log to a custom file.

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
3. `W_BONUS_SELF`
4. `W_BONUS_OPP`
5. `W_RESERVED_SELF`
6. `W_RESERVED_OPP`
7. `W_NOBLE_PROGRESS_SELF`
8. `W_NOBLE_PROGRESS_OPP`
9. `W_AFFORDABLE_SELF`
10. `W_AFFORDABLE_OPP`
11. `W_WIN_BONUS`
12. `W_LOSS_PENALTY`
13. `W_TURN_PENALTY`
14. `W_EFFICIENCY`
15. `W_DIR_FOCUS`
16. `W_DIR_PROGRESS`
17. `W_DIR_SPREAD`
18. `W_DIR_RESERVE_MATCH`
19. `W_DIR_SUPPORT_MATCH`
20. `W_DIR_SLOT_PENALTY`

Example:

```bash
./build/mcts_engine_01 --sims 3000 28.866458 19.799814 1.2506351 1.443658 0.53048973 0.66951027 0.84936494 0.53619669 0.43619669 0.93015465 761.21519 761.21519 0.020127013 1.4433229 1.0695103 0.53475515 0.74865721 0.74865721 0.64170618 1.0695103
```

### Weight Optimization (SPSA)

```bash
python3 scripts/weight_optimizer.py
```

By default, the optimizer runs referee with `--no-log` to avoid referee log I/O during tuning.
Use `--referee-log` if you want to force referee log files during optimization.

### Tournament / Match Running

```bash
python3 scripts/tournament_runner.py ./build/mcts_engine_01 ./build/random_engine
```

To pass referee flags (for example disabling referee file logging):

```bash
python3 scripts/tournament_runner.py "./build/mcts_engine_01" "./build/random_engine" --referee-no-log
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
