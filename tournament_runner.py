import subprocess
import sys
import json
import time

def run_tournament(ref_cmd, engine1_cmd, engine2_cmd):
    # Start the referee process
    # We use pipes to communicate with the referee's STDIN and capture its STDOUT
    referee = subprocess.Popen(
        ref_cmd,
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        bufsize=1
    )

    # Start the engine processes
    engines = [
        subprocess.Popen(engine1_cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=sys.stderr, text=True, bufsize=1),
        subprocess.Popen(engine2_cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=sys.stderr, text=True, bufsize=1)
    ]

    print(f"Tournament started: {engine1_cmd[1]} vs {engine2_cmd[1]}")

    try:
        while True:
            # 1. Read two game states from the referee (one for each player)
            for _ in range(2):
                line = referee.stdout.readline()
                if not line:
                    return

                if line.startswith("WINNER:") or line.startswith("RESULT:"):
                    print(f"Game Over! {line.strip()}")
                    for remaining in referee.stdout:
                        print(remaining.strip())
                    return

                try:
                    state = json.loads(line)
                    viewer_id = state["you"]
                    current_player_id = state["active_player_id"]
                    
                    # Forward the state to the engine it belongs to
                    engines[viewer_id - 1].stdin.write(line)
                    engines[viewer_id - 1].stdin.flush()
                    
                    # Store the state of the active player to know who to read from
                    if viewer_id == current_player_id:
                        active_player_idx = viewer_id - 1
                        current_turn = state["move"]
                except Exception as e:
                    print(f"[REF LOG]: {line.strip()}")
                    continue

            # 2. Get the move from the active engine
            print(f"Turn {current_turn}: Player {active_player_idx + 1}'s move...")
            move = engines[active_player_idx].stdout.readline()
            
            if not move:
                print(f"Error: Player {active_player_idx + 1} disconnected.")
                break

            print(f"Player {active_player_idx + 1} played: {move.strip()}")

            # 3. Send the move back to the referee
            referee.stdin.write(move)
            referee.stdin.flush()

    except KeyboardInterrupt:
        print("\nTournament terminated by user.")
    finally:
        referee.terminate()
        for e in engines:
            e.terminate()

if __name__ == "__main__":
    # Command to run your referee
    ref_call = ["./referee", "12345"] # Seed 12345
    
    # Commands to run your engines with log file arguments
    p1_call = ["python3", "random_engine.py", "engine1.log"]
    p2_call = ["python3", "random_engine.py", "engine2.log"]

    run_tournament(ref_call, p1_call, p2_call)
