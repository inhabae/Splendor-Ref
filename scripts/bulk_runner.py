import subprocess
import json
import multiprocessing
import time
import sys
import os

def run_single_game(args):
    game_id, engine1_path, engine2_path, referee_path, seed = args
    
    # We use a unique seed for each game
    ref_cmd = [referee_path, str(seed)]
    
    referee = subprocess.Popen(
        ref_cmd,
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
        text=True,
        bufsize=1
    )

    # Use DEVNULL for stderr to keep it quiet
    e1 = subprocess.Popen([engine1_path, "ignore"], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True, bufsize=1)
    e2 = subprocess.Popen([engine2_path, "ignore"], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True, bufsize=1)
    
    engines = [e1, e2]
    
    turns = 0
    try:
        while True:
            # Read state from referee
            states = []
            for _ in range(2):
                line = referee.stdout.readline()
                if not line: break
                if line.startswith("WINNER:"):
                    winner_line = line.strip()
                    reason_line = referee.stdout.readline().strip()
                    # Robust parsing for "WINNER: Player X"
                    import re
                    match = re.search(r"Player (\d+)", winner_line)
                    winner_id = int(match.group(1)) if match else -1
                    return {"game_id": game_id, "winner": winner_id, "reason": reason_line, "p1": engine1_path, "p2": engine2_path, "turns": turns}
                if line.startswith("RESULT:"):
                    result_line = line.strip()
                    if "TIE" in result_line:
                        return {"game_id": game_id, "winner": 0, "reason": "Tie game", "p1": engine1_path, "p2": engine2_path, "turns": turns}
                    return {"game_id": game_id, "winner": -1, "reason": result_line, "p1": engine1_path, "p2": engine2_path, "turns": turns}
                
                try:
                    state = json.loads(line)
                    states.append(state)
                except:
                    continue
            
            if not states: break
            
            # Identify active player
            active_p_id = states[0]["active_player_id"]
            # The states are for player 1 and player 2. 
            # state["you"] identifies which player the state is for.
            
            active_engine_idx = active_p_id - 1
            
            # Send state to both engines (though only one really needs it to move)
            for s in states:
                target_idx = s["you"] - 1
                engines[target_idx].stdin.write(json.dumps(s, separators=(',', ':')) + "\n")
                engines[target_idx].stdin.flush()
            
            # Get move
            move = engines[active_engine_idx].stdout.readline()
            if not move: break
            
            # Send move to referee
            referee.stdin.write(move)
            referee.stdin.flush()
            turns += 1
            
    finally:
        referee.terminate()
        e1.terminate()
        e2.terminate()
    
    return {"game_id": game_id, "winner": -2, "p1": engine1_path, "p2": engine2_path, "turns": turns} # Error case

def main():
    if len(sys.argv) < 4:
        print("Usage: python3 bulk_runner.py <num_games> <engine1_path> <engine2_path>")
        sys.exit(1)
    
    total_games = int(sys.argv[1])
    engine1_path = sys.argv[2]
    engine2_path = sys.argv[3]
    
    # Extract names for reporting
    engine1_name = os.path.basename(engine1_path)
    engine2_name = os.path.basename(engine2_path)

    num_p1_is_e1 = total_games // 2
    num_p1_is_e2 = total_games - num_p1_is_e1
    
    tasks = []
    # Seed starts at current time
    base_seed = int(time.time())
    
    # Half games Engine 1 is P1
    for i in range(num_p1_is_e1):
        tasks.append((i, engine1_path, engine2_path, "./build/referee", base_seed + i))
    
    # Half games Engine 2 is P1
    for i in range(num_p1_is_e2):
        tasks.append((num_p1_is_e1 + i, engine2_path, engine1_path, "./build/referee", base_seed + num_p1_is_e1 + i))
    
    print(f"Running {total_games} games: {engine1_name} vs {engine2_name}")
    print(f"Workers: {multiprocessing.cpu_count()} (this may take a few minutes)...", flush=True)
    
    start_time = time.time()
    results = []
    with multiprocessing.Pool(processes=multiprocessing.cpu_count()) as pool:
        for r in pool.imap_unordered(run_single_game, tasks):
            results.append(r)
            print(f"Progress: {len(results)}/{total_games} games completed...", end='\r', flush=True)
    
    print(f"\nCompleted {total_games} games.")
    e1_wins = 0
    e2_wins = 0
    ties = 0
    errors = 0
    invalid_moves = 0
    total_turns = 0
    
    for r in results:
        winner = r["winner"]
        p1 = r["p1"]
        p2 = r["p2"]
        reason = r.get("reason", "")
        total_turns += r.get("turns", 0)
        
        is_invalid = "invalid move" in reason.lower()
        if is_invalid:
            invalid_moves += 1
            if invalid_moves <= 5:
                # Who made the invalid move? The loser.
                loser_id = 1 if winner == 2 else 2
                loser_path = p1 if loser_id == 1 else p2
                loser_name = os.path.basename(loser_path)
                print(f"Game {r['game_id']} INVALID move by {loser_name}. Reason: {reason}")

        if winner == 0:
            ties += 1
        elif winner == 1:
            if p1 == engine1_path: e1_wins += 1
            else: e2_wins += 1
        elif winner == 2:
            if p2 == engine1_path: e1_wins += 1
            else: e2_wins += 1
        else:
            errors += 1
            
    end_time = time.time()
    
    print("\n" + "="*40)
    print(f"Results for {total_games} games:")
    print(f"{engine1_name:15} Wins: {e1_wins:4} ({e1_wins/total_games*100:5.1f}%)")
    print(f"{engine2_name:15} Wins: {e2_wins:4} ({e2_wins/total_games*100:5.1f}%)")
    print(f"{'Ties/Draws':15} Wins: {ties:4} ({ties/total_games*100:5.1f}%)")
    print(f"{'Invalid Moves':15} count: {invalid_moves:4}")
    print(f"{'Avg Turns/Game':15} value: {total_turns/total_games:6.1f}")
    if errors:
        print(f"{'Errors':15} count: {errors:4}")
    print(f"Total Time: {end_time - start_time:12.2f} seconds")
    print(f"Avg Time/Game: {(end_time - start_time)/total_games:9.2f} seconds")
    print("="*40)

if __name__ == "__main__":
    main()
