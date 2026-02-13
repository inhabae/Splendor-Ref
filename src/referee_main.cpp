// Referee - Default Mode
// This executable runs the normal referee mode with random seed

#include <chrono>
#include <iomanip>
#include "game_logic.h"

using std::string;
using std::vector;
using std::map;
using std::cout;
using std::cerr;
using std::endl;
using std::cin;
using std::getline;
using std::to_string;
using std::pair;
using std::make_pair;
using std::stringstream;
using std::istringstream;
using std::ostringstream;
using std::ostream;
using std::istream;
using std::ifstream;
using std::ofstream;
using std::sort;
using std::find;
using std::min;
using std::max;
using std::shuffle;
using std::mt19937;
using std::random_device;
using std::uniform_int_distribution;
using std::runtime_error;
using std::exception;
using std::atoi;


int main(int argc, char* argv[]) {
    GameState game;
    game.replay_mode = false;
    
    // Buffer for logging to prevent engines from reading game info during the match
    stringstream log_ss;

    // Paths can be customized via environment variables or CLI in the future
    const string cards_path = "data/cards.json";
    const string nobles_path = "data/nobles.json";
    
    // Load all cards and nobles to check if they exist
    vector<Card> all_cards = loadCards(cards_path);
    vector<Noble> all_nobles = loadNobles(nobles_path);
    
    if (all_cards.empty() || all_nobles.empty()) {
        cerr << "ERROR: Failed to load game data" << endl;
        return 1;
    }
    
    cerr << "Loaded " << all_cards.size() << " cards and " << all_nobles.size() << " nobles" << endl;
    
    // Normal mode with optional seed
    unsigned int seed = 0;
    if (argc > 1) {
        seed = static_cast<unsigned int>(atoi(argv[1]));
    }
    
    if (seed == 0) {
        seed = static_cast<unsigned int>(time(nullptr));
    }

    initializeGame(game, seed, cards_path, nobles_path);
    
    // Log the seed at the top
    log_ss << "Seed: " << seed << endl;
    
    // Validate initial game state
    ValidationResult validation = validateGameState(game);
    if (!validation.valid) {
        cerr << "ERROR: Invalid game state - " << validation.error_message << endl;
        return 1;
    }
    cerr << "Game state validated successfully" << endl;
    
    // Log initial state
    log_ss << "Initial State: " << gameStateToJson(game, 0) << endl;

    // Output initial game states to both players
    printJsonGameState(game, 1);
    printJsonGameState(game, 2);
    
    cerr << "\n=== Starting Game Loop ===" << endl;
    
    while (!isGameOver(game)) {
        int current = game.current_player;
        cerr << "\nWaiting for Player " << (current + 1) << " move (Bank: " 
             << std::fixed << std::setprecision(3) << game.players[current].time_bank << "s)..." << endl;
        
        // Start timer
        auto start_time = std::chrono::steady_clock::now();
        
        // Read move from STDIN
        string move_string;
        if (!getline(cin, move_string)) {
            cerr << "ERROR: Failed to read move from STDIN" << endl;
            break;
        }
        
        // Stop timer and update bank
        auto end_time = std::chrono::steady_clock::now();
        std::chrono::duration<double> elapsed = end_time - start_time;
        
        game.players[current].time_bank -= elapsed.count();
        
        // Check for timeout
        if (game.players[current].time_bank < 0) {
            cerr << "ERROR: Player " << (current + 1) << " timed out!" << endl;
            log_ss << "ERROR: Player " << (current + 1) << " timed out!" << endl;
            log_ss << "Game Result: Player " << (2 - current) << " wins! (Opponent timeout)" << endl;
            
            cout << "WINNER: Player " << (2 - current) << endl;
            cout << "REASON: Player " << (current + 1) << " timed out (" 
                 << std::fixed << std::setprecision(3) << game.players[current].time_bank << "s)" << endl;
            
            // Write log and exit
            ofstream log_file("game.log");
            if (log_file.is_open()) {
                log_file << log_ss.str();
                log_file.close();
            }
            return 0;
        }
        
        // Add move increment
        game.players[current].time_bank += TIME_INCREMENT;
        
        cerr << "Received move: \"" << move_string << "\" (Took " 
             << std::fixed << std::setprecision(3) << elapsed.count() << "s)" << endl;
        
        // Log the move
        log_ss << "Player " << (current + 1) << ": " << move_string << endl;

        // REVEAL commands not allowed in normal mode
        if (move_string.find("REVEAL") == 0) {
            cerr << "ERROR: REVEAL command only valid in replay mode" << endl;
            continue;
        }
        
        // Parse the move
        auto parse_result = parseMove(move_string, current);
        Move move = parse_result.first;
        ValidationResult move_valid = parse_result.second;
        
        if (!move_valid.valid) {
            cerr << "ERROR: Parse error - " << move_valid.error_message << endl;
        } else {
            // Validate the move logic
            move_valid = validateMove(game, move);
        }
        
        if (!move_valid.valid) {
            cerr << "ERROR: Invalid move - " << move_valid.error_message << endl;
            cerr << "Player " << (current + 1) << " loses by invalid move" << endl;
            
            log_ss << "ERROR: Invalid move from Player " << (current + 1) << ": " << move_valid.error_message << endl;
            log_ss << "Game Result: Player " << (2 - current) << " wins! (Opponent invalid move)" << endl;

            // Output result: opponent wins
            cout << "WINNER: Player " << (2 - current) << endl;
            cout << "REASON: Player " << (current + 1) << " made invalid move (" << move_valid.error_message << ")" << endl;
            
            // Write log and exit
            ofstream log_file("game.log");
            if (log_file.is_open()) {
                log_file << log_ss.str();
                log_file.close();
            }
            return 0;
        }
        
        // Apply the move
        ValidationResult apply_result = applyMove(game, move);
        if (!apply_result.valid) {
            cerr << "ERROR: Failed to apply move - " << apply_result.error_message << endl;
            return 1;
        }
        cerr << "Move applied successfully" << endl;

        // Log the state after the move to capture any revealed cards
        log_ss << "Post-Move State: " << gameStateToJson(game, 0) << endl;
        
        // Validate game state after move
        ValidationResult validation_after = validateGameState(game);
        if (!validation_after.valid) {
            cerr << "ERROR: Game state became invalid - " << validation_after.error_message << endl;
            return 1;
        }
        
        // Output updated game states to both players if game is not over
        if (!isGameOver(game)) {
            printJsonGameState(game, 1);
            printJsonGameState(game, 2);
        }
    }
    
    // Game ended - determine winner
    cerr << "\n=== Game Over ===" << endl;
    int winner = determineWinner(game);
    
    cerr << "Final Scores:" << endl;
    cerr << "  Player 1: " << game.players[0].points << " points, " 
         << game.players[0].cards.size() << " cards" << endl;
    cerr << "  Player 2: " << game.players[1].points << " points, " 
         << game.players[1].cards.size() << " cards" << endl;
    
    // Output winner
    if (winner == -1) {
        cout << "RESULT: TIE" << endl;
        cerr << "Game ended in a tie" << endl;
        log_ss << "RESULT: TIE" << endl;
    } else {
        cout << "WINNER: Player " << (winner + 1) << endl;
        cerr << "Player " << (winner + 1) << " wins!" << endl;
        log_ss << "WINNER: Player " << (winner + 1) << endl;
    }

    // Reveal the seed to engines at the end of the game
    cout << "SEED: " << seed << endl;

    log_ss << "Final Scores - P1: " << game.players[0].points << ", P2: " << game.players[1].points << endl;
    if (winner == -1) {
        log_ss << "Game Result: TIE" << endl;
    } else {
        log_ss << "Game Result: Player " << (winner + 1) << " wins!" << endl;
    }
    
    // Write the buffered log to game.log after the game is over
    ofstream log_file("game.log");
    if (log_file.is_open()) {
        log_file << log_ss.str();
        log_file.close();
    }
    
    return 0;
}
