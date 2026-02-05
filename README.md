# Splendor-Ref

A language-agnostic **Referee** and technical specification for Splendor AI development.

### Documentation

For requirements, communication protocols, and system architecture, visit:
**[Splendor Technical Specification Doc](https://docs.google.com/document/d/1Kaxwhw_BPadKAm-6Yx3AHF48tGFeXDjUvHSWNuZoMf0/edit?tab=t.0)**

### Resources

Use these standardized files for engine development:

* **`cards.json`**: Data for all 90 development cards.
* **`nobles.json`**: Data for all 10 noble patrons.
* https://splendor-card-lookup.vercel.app/: Tool for easy card/noble lookup

### Development & Testing

#### 1. Referee (`referee_main.cpp`)
The referee handles game logic, state JSON broadcasting, and time control.
```bash
make referee
./referee [seed] [cards_path] [nobles_path]
```
Communication is via JSON-over-STDIN/STDOUT. Every turn, both players receive the full game state.

#### 2. Tournament Runner (`tournament_runner.py`)
Testing utility to run matches between two engine processes.
```bash
python3 tournament_runner.py
```
As of now, it runs `random_engine.py` vs itself. Perfect for verifying engine stability and turn handling.

#### 3. Core Logic (`game_logic.cpp`)
C++ engines can link directly against `game_logic.o` to reuse official rule validation and state transitions. See `game_logic.h` for the API.
