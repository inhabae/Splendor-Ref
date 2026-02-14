#!/usr/bin/env python3

import argparse
import multiprocessing
import random
import shlex
from typing import List, Sequence, Tuple

import tournament_runner as tr


WEIGHT_NAMES: List[str] = [
    "W_POINT_SELF",
    "W_POINT_OPP",
    "W_GEM_SELF",
    "W_GEM_OPP",
    "W_BONUS_SELF",
    "W_BONUS_OPP",
    "W_RESERVED_SELF",
    "W_RESERVED_OPP",
    "W_NOBLE_PROGRESS_SELF",
    "W_NOBLE_PROGRESS_OPP",
    "W_AFFORDABLE_SELF",
    "W_AFFORDABLE_OPP",
    "W_WIN_BONUS",
    "W_LOSS_PENALTY",
    "W_TURN_PENALTY",
    "W_EFFICIENCY",
    "W_DIRECTIONAL_COMMITMENT",
]

DEFAULT_WEIGHTS: List[float] = [
    20.0,
    20.0,
    0.25,
    0.25,
    1.2,
    1.2,
    0.6,
    0.6,
    0.9,
    0.9,
    0.8,
    0.8,
    1000.0,
    1000.0,
    0.01,
    1.0,
    1.0,
]

BOUNDS: List[Tuple[float, float]] = [
    (0.0, 100.0),
    (0.0, 100.0),
    (0.0, 5.0),
    (0.0, 5.0),
    (0.0, 5.0),
    (0.0, 5.0),
    (0.0, 5.0),
    (0.0, 5.0),
    (0.0, 5.0),
    (0.0, 5.0),
    (0.0, 5.0),
    (0.0, 5.0),
    (100.0, 5000.0),
    (100.0, 5000.0),
    (0.0, 1.0),
    (0.0, 5.0),
    (0.0, 5.0),
]


def parse_weights(text: str) -> List[float]:
    raw = [p.strip() for p in text.split(",") if p.strip()]
    values = [float(x) for x in raw]
    if len(values) != len(WEIGHT_NAMES):
        raise ValueError(f"Expected {len(WEIGHT_NAMES)} weights, got {len(values)}")
    return values


def to_unit(weights: Sequence[float]) -> List[float]:
    unit: List[float] = []
    for idx, value in enumerate(weights):
        lo, hi = BOUNDS[idx]
        clamped = min(max(value, lo), hi)
        if hi <= lo:
            unit.append(0.0)
        else:
            unit.append((clamped - lo) / (hi - lo))
    return unit


def from_unit(unit: Sequence[float]) -> List[float]:
    weights: List[float] = []
    for idx, u in enumerate(unit):
        lo, hi = BOUNDS[idx]
        uc = min(max(u, 0.0), 1.0)
        weights.append(lo + uc * (hi - lo))
    return weights


def build_cmd(engine: str, engine_args: Sequence[str], weights: Sequence[float]) -> List[str]:
    return [engine] + list(engine_args) + [f"{w:.8g}" for w in weights]


def run_tasks(tasks: Sequence[Tuple], workers: int) -> List[tr.GameResult]:
    if workers <= 1:
        return [tr.run_single_game(t) for t in tasks]
    with multiprocessing.Pool(processes=workers) as pool:
        return list(pool.imap_unordered(tr.run_single_game, tasks))


def candidate_score(results: Sequence[tr.GameResult], candidate_cmd: Sequence[str]) -> float:
    candidate_key = " ".join(candidate_cmd)
    total = 0.0
    if not results:
        return 0.0

    for r in results:
        p1 = " ".join(r.p1_cmd)
        p2 = " ".join(r.p2_cmd)
        candidate_side = 1 if p1 == candidate_key else (2 if p2 == candidate_key else 0)
        if candidate_side == 0:
            continue

        if r.winner == candidate_side:
            total += 1.0
        elif r.winner == 0:
            total += 0.5
        elif r.winner < 0:
            total -= 0.25

    return total / float(len(results))


def evaluate_weights(
    weights: Sequence[float],
    baseline_cmd: Sequence[str],
    args: argparse.Namespace,
    seed: int,
) -> float:
    candidate_cmd = build_cmd(args.engine, args.engine_args, weights)
    tasks = []
    for game_id in range(args.games_per_eval):
        p1_cmd = list(candidate_cmd)
        p2_cmd = list(baseline_cmd)
        if args.swap_sides and (game_id % 2 == 1):
            p1_cmd, p2_cmd = p2_cmd, p1_cmd
        tasks.append(
            (
                game_id,
                p1_cmd,
                p2_cmd,
                args.referee,
                args.referee_args,
                seed + game_id,
                args.log_games,
                args.log_dir,
                False,
            )
        )

    results = run_tasks(tasks, args.parallel)
    return candidate_score(results, candidate_cmd)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="SPSA optimizer for mcts_engine_01 evaluation weights (non-GA)."
    )
    parser.add_argument("--engine", default="./build/mcts_engine_01", help="Engine executable")
    parser.add_argument(
        "--engine-args",
        default="--sims 3000 --det 8 --max-depth 18 --c-puct 1.25 --risk-lambda 0.30",
        help="Quoted args passed before positional weights",
    )
    parser.add_argument("--referee", default="./build/referee", help="Referee executable")
    parser.add_argument(
        "--referee-args",
        default="--no-log",
        help="Quoted extra args passed to referee before seed (default disables referee log file writes)",
    )
    parser.add_argument(
        "--referee-log",
        action="store_true",
        help="Enable referee file logging during optimization",
    )
    parser.add_argument(
        "--baseline-cmd",
        default=None,
        help="Full opponent command. If omitted, uses same engine with default/baseline weights.",
    )
    parser.add_argument(
        "--baseline-weights",
        default=None,
        help="Comma-separated 17 values used only when --baseline-cmd is omitted",
    )
    parser.add_argument(
        "--init-weights",
        default=None,
        help="Comma-separated 17 values to start optimization from (default: built-in defaults)",
    )
    parser.add_argument("--iterations", type=int, default=30, help="SPSA iterations")
    parser.add_argument("--games-per-eval", type=int, default=24, help="Games per objective eval")
    parser.add_argument("--final-games", type=int, default=96, help="Games for final best check")
    parser.add_argument("--seed", type=int, default=42, help="Random seed")
    parser.add_argument("--parallel", type=int, default=4, help="Worker count")
    parser.add_argument("--no-swap-sides", action="store_false", dest="swap_sides")
    parser.set_defaults(swap_sides=True)
    parser.add_argument("--log-games", action="store_true", help="Store game logs")
    parser.add_argument("--log-dir", default="./logs", help="Log directory")
    parser.add_argument("--a", type=float, default=0.08, help="SPSA step-size coefficient")
    parser.add_argument("--c", type=float, default=0.10, help="SPSA perturbation coefficient")
    parser.add_argument("--A", type=float, default=10.0, help="SPSA stability constant")
    parser.add_argument("--alpha", type=float, default=0.602, help="SPSA alpha")
    parser.add_argument("--gamma", type=float, default=0.101, help="SPSA gamma")
    args = parser.parse_args()

    args.engine_args = shlex.split(args.engine_args)
    args.referee_args = shlex.split(args.referee_args)
    if not args.referee_log and "--no-log" not in args.referee_args:
        args.referee_args.append("--no-log")
    if args.referee_log:
        args.referee_args = [a for a in args.referee_args if a != "--no-log"]
    if args.iterations <= 0 or args.games_per_eval <= 0 or args.final_games <= 0:
        print("iterations, games-per-eval, and final-games must be > 0")
        return 2

    rng = random.Random(args.seed)

    init_weights = DEFAULT_WEIGHTS if args.init_weights is None else parse_weights(args.init_weights)
    baseline_weights = (
        DEFAULT_WEIGHTS if args.baseline_weights is None else parse_weights(args.baseline_weights)
    )

    if args.baseline_cmd is not None:
        baseline_cmd = tr.parse_engine_cmd(args.baseline_cmd)
    else:
        baseline_cmd = build_cmd(args.engine, args.engine_args, baseline_weights)

    u = to_unit(init_weights)
    best_u = list(u)
    best_score = evaluate_weights(from_unit(best_u), baseline_cmd, args, args.seed * 1000)

    print(f"Initial score vs baseline: {best_score:.4f}")

    for k in range(args.iterations):
        ak = args.a / ((k + 1 + args.A) ** args.alpha)
        ck = args.c / ((k + 1) ** args.gamma)

        delta = [1.0 if rng.random() < 0.5 else -1.0 for _ in range(len(WEIGHT_NAMES))]
        u_plus = [min(1.0, max(0.0, u[i] + ck * delta[i])) for i in range(len(u))]
        u_minus = [min(1.0, max(0.0, u[i] - ck * delta[i])) for i in range(len(u))]

        iter_seed = args.seed * 100000 + (k * 2000)
        y_plus = evaluate_weights(from_unit(u_plus), baseline_cmd, args, iter_seed)
        y_minus = evaluate_weights(from_unit(u_minus), baseline_cmd, args, iter_seed)

        grad = [(y_plus - y_minus) / (2.0 * ck * delta[i]) for i in range(len(u))]
        u = [min(1.0, max(0.0, u[i] + ak * grad[i])) for i in range(len(u))]
        y_new = evaluate_weights(from_unit(u), baseline_cmd, args, iter_seed + 1000)

        if y_plus > best_score:
            best_score, best_u = y_plus, list(u_plus)
        if y_minus > best_score:
            best_score, best_u = y_minus, list(u_minus)
        if y_new > best_score:
            best_score, best_u = y_new, list(u)

        print(
            f"iter={k+1:02d} ak={ak:.4f} ck={ck:.4f} "
            f"y+={y_plus:.4f} y-={y_minus:.4f} y={y_new:.4f} best={best_score:.4f}"
        )

    best_weights = from_unit(best_u)
    final_args = argparse.Namespace(**vars(args))
    final_args.games_per_eval = args.final_games
    final_score = evaluate_weights(best_weights, baseline_cmd, final_args, args.seed * 999999)

    print("\nBest weights:")
    for name, value in zip(WEIGHT_NAMES, best_weights):
        print(f"  {name}={value:.8g}")

    print(f"\nFinal score over {args.final_games} games: {final_score:.4f}")
    print("Command:")
    print(" ".join(build_cmd(args.engine, args.engine_args, best_weights)))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
