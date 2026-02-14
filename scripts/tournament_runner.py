#!/usr/bin/env python3

import argparse
import json
import multiprocessing
import os
import re
import shlex
import subprocess
import time
from dataclasses import dataclass
from typing import List, Optional, Tuple


@dataclass
class GameResult:
    game_id: int
    seed: int
    p1_cmd: List[str]
    p2_cmd: List[str]
    winner: int  # 1, 2, 0(tie), negative(error)
    reason: str
    turns: int
    error: str = ""


def parse_engine_cmd(raw: str) -> List[str]:
    cmd = shlex.split(raw)
    if not cmd:
        raise ValueError(f"Empty engine command: {raw}")
    return cmd


def read_line(proc: subprocess.Popen) -> str:
    if proc.stdout is None:
        return ""
    return proc.stdout.readline()


def write_line(proc: subprocess.Popen, line: str) -> None:
    if proc.stdin is None:
        return
    proc.stdin.write(line)
    proc.stdin.flush()


def make_log_path(log_dir: str, game_id: int, seed: int) -> str:
    return os.path.join(log_dir, f"game_{game_id:04d}_seed_{seed}.log")


def run_single_game(task: Tuple[int, List[str], List[str], str, int, bool, str, bool]) -> GameResult:
    game_id, p1_cmd, p2_cmd, referee_path, seed, enable_log, log_dir, verbose = task

    log_lines: List[str] = []
    log_path: Optional[str] = None
    if enable_log:
        resolved_log_dir = log_dir
        try:
            os.makedirs(resolved_log_dir, exist_ok=True)
        except OSError:
            resolved_log_dir = os.path.join(os.getcwd(), "logs")
            os.makedirs(resolved_log_dir, exist_ok=True)
        log_path = make_log_path(resolved_log_dir, game_id, seed)

    referee = subprocess.Popen(
        [referee_path, str(seed)],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
        text=True,
        bufsize=1,
    )

    e1 = subprocess.Popen(
        p1_cmd,
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
        text=True,
        bufsize=1,
    )
    e2 = subprocess.Popen(
        p2_cmd,
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
        text=True,
        bufsize=1,
    )

    engines = [e1, e2]
    turns = 0

    if enable_log:
        log_lines.append(f"[GAME] id={game_id} seed={seed}")
        log_lines.append(f"[SETUP] P1={' '.join(p1_cmd)}")
        log_lines.append(f"[SETUP] P2={' '.join(p2_cmd)}")

    def finalize(result: GameResult) -> GameResult:
        if enable_log and log_path is not None:
            with open(log_path, "w", encoding="utf-8") as f:
                f.write("\n".join(log_lines) + "\n")
        return result

    try:
        while True:
            states = []

            for _ in range(2):
                line = read_line(referee)
                if not line:
                    return finalize(
                        GameResult(game_id, seed, p1_cmd, p2_cmd, -2, "Referee disconnected", turns)
                    )

                text = line.rstrip("\n")
                if enable_log:
                    try:
                        parsed = json.loads(text)
                        viewer = int(parsed.get("you", 0))
                        if viewer in (1, 2):
                            log_lines.append(f"[REF P{viewer}] {text}")
                        else:
                            log_lines.append(f"[REF] {text}")
                    except Exception:
                        log_lines.append(f"[REF] {text}")

                if text.startswith("WINNER:"):
                    match = re.search(r"Player\s+(\d+)", text)
                    winner = int(match.group(1)) if match else -1
                    reason = read_line(referee).strip()
                    if enable_log and reason:
                        log_lines.append(f"[REF] {reason}")
                    return finalize(
                        GameResult(game_id, seed, p1_cmd, p2_cmd, winner, reason, turns)
                    )

                if text.startswith("RESULT:"):
                    winner = 0 if "TIE" in text else -1
                    return finalize(
                        GameResult(game_id, seed, p1_cmd, p2_cmd, winner, text, turns)
                    )

                try:
                    states.append(json.loads(text))
                except Exception:
                    continue

            if not states:
                return finalize(
                    GameResult(game_id, seed, p1_cmd, p2_cmd, -3, "No playable state received", turns)
                )

            active_player = states[0].get("active_player_id", 1)
            active_idx = max(1, min(2, int(active_player))) - 1

            for s in states:
                viewer_idx = int(s.get("you", 1)) - 1
                payload = json.dumps(s, separators=(",", ":")) + "\n"
                write_line(engines[viewer_idx], payload)

            move = read_line(engines[active_idx])
            if not move:
                return finalize(
                    GameResult(game_id, seed, p1_cmd, p2_cmd, -4, f"Engine P{active_idx+1} disconnected", turns)
                )

            if enable_log:
                log_lines.append(f"[MOVE P{active_idx+1}] {move.rstrip()}")

            if verbose:
                print(f"Game {game_id} Turn {turns + 1}: P{active_idx+1} -> {move.strip()}")

            write_line(referee, move)
            turns += 1

    except Exception as e:
        return finalize(
            GameResult(game_id, seed, p1_cmd, p2_cmd, -5, "Exception", turns, error=str(e))
        )
    finally:
        referee.terminate()
        e1.terminate()
        e2.terminate()


def summarize(results: List[GameResult], engine1_cmd: List[str], engine2_cmd: List[str]) -> None:
    e1_wins = 0
    e2_wins = 0
    ties = 0
    errors = 0
    invalid_moves = 0
    total_turns = 0

    e1_key = " ".join(engine1_cmd)
    e2_key = " ".join(engine2_cmd)

    for r in results:
        total_turns += r.turns

        reason_l = (r.reason or "").lower()
        if "invalid move" in reason_l:
            invalid_moves += 1

        if r.winner == 0:
            ties += 1
            continue

        if r.winner < 0:
            errors += 1
            continue

        p1_key = " ".join(r.p1_cmd)
        p2_key = " ".join(r.p2_cmd)

        if r.winner == 1:
            if p1_key == e1_key:
                e1_wins += 1
            elif p1_key == e2_key:
                e2_wins += 1
        elif r.winner == 2:
            if p2_key == e1_key:
                e1_wins += 1
            elif p2_key == e2_key:
                e2_wins += 1

    n = len(results)
    if n == 0:
        print("No games completed.")
        return

    print("\n" + "=" * 50)
    print(f"Games: {n}")
    print(f"Engine1 wins: {e1_wins} ({(e1_wins / n) * 100:.1f}%)")
    print(f"Engine2 wins: {e2_wins} ({(e2_wins / n) * 100:.1f}%)")
    print(f"Ties: {ties} ({(ties / n) * 100:.1f}%)")
    print(f"Invalid moves: {invalid_moves}")
    print(f"Errors: {errors}")
    print(f"Avg turns/game: {total_turns / n:.1f}")
    print("=" * 50)


def build_tasks(args: argparse.Namespace, engine1_cmd: List[str], engine2_cmd: List[str]) -> List[Tuple[int, List[str], List[str], str, int, bool, str, bool]]:
    tasks = []
    base_seed = args.seed if args.seed is not None else int(time.time())

    for game_id in range(args.games):
        p1_cmd = list(engine1_cmd)
        p2_cmd = list(engine2_cmd)

        if args.swap_sides and (game_id % 2 == 1):
            p1_cmd, p2_cmd = p2_cmd, p1_cmd

        task = (
            game_id,
            p1_cmd,
            p2_cmd,
            args.referee,
            base_seed + game_id,
            args.log,
            args.log_dir,
            args.verbose,
        )
        tasks.append(task)

    return tasks


def run_cli(argv: Optional[List[str]] = None) -> int:
    parser = argparse.ArgumentParser(
        description="Unified engine-vs-engine runner (single game and bulk modes)."
    )
    parser.add_argument("engine1", help="Engine 1 command (quote if it has args)")
    parser.add_argument("engine2", help="Engine 2 command (quote if it has args)")
    parser.add_argument("--games", type=int, default=1, help="Number of games to run (default: 1)")
    parser.add_argument("--referee", default="./build/referee", help="Referee executable path")
    parser.add_argument("--seed", type=int, default=None, help="Base seed (game i uses seed+i)")
    parser.add_argument(
        "--parallel",
        type=int,
        default=None,
        help="Worker count for bulk runs (default: cpu count for games>1)",
    )
    parser.add_argument(
        "--no-swap-sides",
        action="store_false",
        dest="swap_sides",
        help="Disable alternating player order between games",
    )
    parser.set_defaults(swap_sides=True)
    parser.add_argument("--log", action="store_true", help="Enable per-game log files")
    parser.add_argument(
        "--log-dir",
        default="/logs",
        help="Directory for logs when --log is enabled (default: /logs, fallback: ./logs)",
    )
    parser.add_argument("--verbose", action="store_true", help="Print per-turn moves")

    args = parser.parse_args(argv)

    if args.games <= 0:
        print("--games must be >= 1")
        return 2

    try:
        engine1_cmd = parse_engine_cmd(args.engine1)
        engine2_cmd = parse_engine_cmd(args.engine2)
    except ValueError as e:
        print(str(e))
        return 2

    tasks = build_tasks(args, engine1_cmd, engine2_cmd)

    start = time.time()
    results: List[GameResult] = []

    if args.games == 1:
        result = run_single_game(tasks[0])
        results.append(result)
        print(f"Winner: {result.winner}, turns: {result.turns}, reason: {result.reason}")
        if result.error:
            print(f"Error: {result.error}")
    else:
        workers = args.parallel or multiprocessing.cpu_count()
        workers = max(1, min(workers, args.games))

        print(f"Running {args.games} games with {workers} worker(s)")
        with multiprocessing.Pool(processes=workers) as pool:
            for idx, result in enumerate(pool.imap_unordered(run_single_game, tasks), start=1):
                results.append(result)
                print(f"Progress: {idx}/{args.games}", end="\r", flush=True)
        print()

    summarize(results, engine1_cmd, engine2_cmd)
    print(f"Elapsed: {time.time() - start:.2f}s")

    if any(r.winner < 0 for r in results):
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(run_cli())
