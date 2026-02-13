import sys
import json
import random

def get_move(state):
    # Extremely simple: always take Red, Green, and White if it is our turn.
    return "TAKE red green white"

def main():
    # Optional logging
    log_file = None
    if len(sys.argv) > 1:
        log_file = open(sys.argv[1], "w")

    for line in sys.stdin:
        try:
            if log_file:
                log_file.write(line)
                log_file.flush()

            state = json.loads(line)
            
            # Only output a move if it's our turn
            if state["active_player_id"] == state["you"]:
                move = get_move(state)
                print(move)
                sys.stdout.flush()
            
        except Exception as e:
            # Errors to stderr so they don't break the protocol
            print(f"Engine Error: {e}", file=sys.stderr)

if __name__ == "__main__":
    main()
