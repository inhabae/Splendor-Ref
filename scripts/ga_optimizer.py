import subprocess
import json
import multiprocessing
import random
import os
import time
import re

# --- Configuration ---
WEIGHT_NAMES = [
    "W_CARD", "W_GEM", "W_JOKER", "W_POINT", "W_NOBLE", 
    "W_NOBLE_PROGRESS", "W_RESERVED_PROGRESS", "W_RESERVED_EFFICIENCY", "W_UNRESERVED_SLOT",
    "W_BOUGHT_EFFICIENCY"
]
POP_SIZE = 12
GENERATIONS = 50
GAMES_PER_BASELINE = 30  # Games per baseline opponent
CONVERGENCE_PATIENCE = 7
FITNESS_IMPROVEMENT_THRESHOLD = 0.01  # Minimum improvement to reset convergence
ENGINE_EXE = "./build/mcts_engine_04"
BASELINES = ["./build/mcts_engine_01", "./build/mcts_engine_03"]
REFEREE_EXE = "./build/referee"

def run_single_game_with_weights(args):
    game_id, engine1_cmd, engine2_cmd, seed = args
    
    ref_cmd = [REFEREE_EXE, str(seed)]
    referee = subprocess.Popen(ref_cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True, bufsize=1)
    
    e1 = subprocess.Popen(engine1_cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True, bufsize=1)
    e2 = subprocess.Popen(engine2_cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True, bufsize=1)
    
    engines = [e1, e2]
    try:
        while True:
            states = []
            for _ in range(2):
                line = referee.stdout.readline()
                if not line: break
                if line.startswith("WINNER:"):
                    match = re.search(r"Player (\d+)", line)
                    return int(match.group(1)) if match else -1
                if line.startswith("RESULT:"):
                    return 0 # Tie
                try: states.append(json.loads(line))
                except: continue
            
            if not states: break
            active_p_id = states[0]["active_player_id"]
            for s in states:
                engines[s["you"]-1].stdin.write(json.dumps(s) + "\n")
                engines[s["you"]-1].stdin.flush()
            
            move = engines[active_p_id-1].stdout.readline()
            if not move: break
            referee.stdin.write(move)
            referee.stdin.flush()
    finally:
        referee.terminate()
        for e in engines: e.terminate()
    return -1

class GAOptimizer:
    def __init__(self):
        # Start with current weights as a base for one individual
        initial_base = [1.11, 0.18, 0.59, 8.73, 4.83, 0.33, 0.39, 0.52, 0.28, 1.0]
        self.population = [self.mutate(list(initial_base), force=True) for _ in range(POP_SIZE)]
        self.population[0] = initial_base  # Keep one exactly as is (no mutation)
        self.best_individual = None
        self.best_fitness = -1.0

    def fitness(self, chromosome):
        tasks = []
        base_seed = random.getrandbits(32) 
        
        # Total games = len(BASELINES) * GAMES_PER_BASELINE
        total_expected_games = len(BASELINES) * GAMES_PER_BASELINE
        
        for b_idx, baseline in enumerate(BASELINES):
            for i in range(GAMES_PER_BASELINE):
                engine_cmd = [ENGINE_EXE] + [str(w) for w in chromosome]
                baseline_cmd = [baseline, "ignore"]
                game_id = b_idx * GAMES_PER_BASELINE + i
                if i % 2 == 0:
                    tasks.append((game_id, engine_cmd, baseline_cmd, base_seed + game_id))
                else:
                    tasks.append((game_id, baseline_cmd, engine_cmd, base_seed + game_id))
        
        wins = 0
        ties = 0
        with multiprocessing.Pool(processes=multiprocessing.cpu_count()) as pool:
            results = pool.map(run_single_game_with_weights, tasks)
            
        for i, winner in enumerate(results):
            # Determine if chromosome was P1 or P2 based on task setup
            if i % 2 == 0: # Chromosome was P1
                if winner == 1: wins += 1
                elif winner == 0: ties += 1
            else: # Chromosome was P2
                if winner == 2: wins += 1
                elif winner == 0: ties += 1
        
        score = (wins + 0.5 * ties) / total_expected_games
        return score

    def mutate(self, individual, force=False):
        MUTATION_RATE = 0.2
        MUTATION_SIGMA_RATIO = 0.2
        MUTATION_SIGMA_MIN = 0.1
        
        new_ind = []
        for val in individual:
            if force or random.random() < MUTATION_RATE:
                sigma = val * MUTATION_SIGMA_RATIO if val != 0 else MUTATION_SIGMA_MIN
                mutation = random.gauss(0, sigma)
                new_ind.append(max(0.0, val + mutation))
            else:
                new_ind.append(val)
        return new_ind

    def crossover(self, p1, p2):
        # Uniform crossover with light blending
        child = []
        for g1, g2 in zip(p1, p2):
            mix = random.random()
            if mix < 0.4: child.append(g1)
            elif mix < 0.8: child.append(g2)
            else: child.append((g1 + g2) / 2) # Blend
        return child

    def evolve(self):
        gens_without_improvement = 0
        for gen in range(GENERATIONS):
            # Seed the overall generation differently
            random.seed(time.time() + gen)
            print(f"\n--- Generation {gen} ---")

            # Re-evaluate the best individual to ensure it's not a fluke
            if self.best_individual is not None:
                print("Re-evaluating current elite individual...")
                new_best_fitness = self.fitness(self.best_individual)
                print(f"Elite re-evaluation: {self.best_fitness:.2f} -> {new_best_fitness:.2f}")
                self.best_fitness = new_best_fitness # Update estimate, but don't reset counter here

            scores = []
            for i, ind in enumerate(self.population):
                score = self.fitness(ind)
                scores.append(score)
                print(f"Individual {i} fitness: {score:.2f}")
            
            # Find best individual in current scores
            best_val = -1
            best_idx = 0
            for i, s in enumerate(scores):
                if s > best_val:
                    best_val = s
                    best_idx = i
            
            # Use a significance threshold to prevent jitter
            if best_val > self.best_fitness + FITNESS_IMPROVEMENT_THRESHOLD: 
                print(f"Improvement found! {self.best_fitness:.2f} -> {best_val:.2f}")
                self.best_fitness = best_val
                self.best_individual = self.population[best_idx]
                gens_without_improvement = 0
            else:
                gens_without_improvement += 1
                print(f"No significant improvement for {gens_without_improvement} generation(s).")
            
            print(f"Gen {gen} Best Overall: {self.best_fitness:.2f}")

            if gens_without_improvement >= CONVERGENCE_PATIENCE:
                print(f"\nConvergence reached. Stopping after {gen} generations.")
                break
            
            # Simple Selection & Reproduction
            new_pop = [self.best_individual] # Elitism
            while len(new_pop) < POP_SIZE:
                # Tournament Selection
                idx1, idx2 = random.sample(range(POP_SIZE), 2)
                parent1 = self.population[idx1] if scores[idx1] > scores[idx2] else self.population[idx2]
                idx3, idx4 = random.sample(range(POP_SIZE), 2)
                parent2 = self.population[idx3] if scores[idx3] > scores[idx4] else self.population[idx4]
                
                child = self.crossover(parent1, parent2)
                child = self.mutate(child)
                new_pop.append(child)
            self.population = new_pop

if __name__ == "__main__":
    print("="*60)
    print("Splendor Weight Optimization via Genetic Algorithm")
    print("="*60)
    print(f"Population Size: {POP_SIZE}")
    print(f"Max Generations: {GENERATIONS}")
    print(f"Games per Baseline: {GAMES_PER_BASELINE}")
    print(f"Total Games per Individual: {len(BASELINES) * GAMES_PER_BASELINE}")
    print(f"Baseline Opponents: {', '.join(BASELINES)}")
    print(f"Convergence Patience: {CONVERGENCE_PATIENCE}")
    print("="*60)
    
    # Verify executables exist
    for exe in [ENGINE_EXE, REFEREE_EXE] + BASELINES:
        if not os.path.exists(exe):
            print(f"ERROR: Executable not found: {exe}")
            print("Please compile/verify all required files before running.")
            exit(1)
    
    print("All executables verified. Starting optimization...\n")
    opt = GAOptimizer()
    opt.evolve()
    
    print("\n" + "="*60)
    print("Optimization Complete!")
    print("="*60)
    print(f"Best Fitness: {opt.best_fitness:.4f}")
    print(f"Best Weights:")
    for i, name in enumerate(WEIGHT_NAMES):
        if opt.best_individual:
            print(f"  {name:25s} = {opt.best_individual[i]:.6f}")
    print("\nTo use these weights, run:")
    if opt.best_individual:
        weight_str = " ".join([f"{w:.6f}" for w in opt.best_individual])
        print(f"  {ENGINE_EXE} {weight_str}")
    print("="*60)
