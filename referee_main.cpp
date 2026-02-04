// Referee - Default Mode
// This executable runs the normal referee mode with random seed

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
    
    // Paths can be customized via environment variables or CLI in the future
    const string cards_path = "cards.json";
    const string nobles_path = "nobles.json";
    
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
    
    initializeGame(game, seed, cards_path, nobles_path);
    
    // Validate initial game state
    ValidationResult validation = validateGameState(game);
    if (!validation.valid) {
        cerr << "ERROR: Invalid game state - " << validation.error_message << endl;
        return 1;
    }
    cerr << "Game state validated successfully" << endl;
    
    // Output initial game states
    cerr << "\n=== Initial Game State for Player 1 ===" << endl;
    printJsonGameState(game, 1);
    
    cerr << "\n=== Initial Game State for Player 2 ===" << endl;
    printJsonGameState(game, 2);
    
    cerr << "\n=== Starting Game Loop ===" << endl;
    
    while (!isGameOver(game)) {
        int current = game.current_player;
        cerr << "\nWaiting for Player " << (current + 1) << " move..." << endl;
        
        // Read move from STDIN
        string move_string;
        if (!getline(cin, move_string)) {
            cerr << "ERROR: Failed to read move from STDIN" << endl;
            break;
        }
        
        cerr << "Received move: \"" << move_string << "\"" << endl;
        
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
            
            // Output result: opponent wins
            cout << "WINNER: Player " << (2 - current) << endl;
            cout << "REASON: Player " << (current + 1) << " made invalid move (" << move_valid.error_message << ")" << endl;
            return 0;
        }
        
        // Apply the move
        ValidationResult apply_result = applyMove(game, move);
        if (!apply_result.valid) {
            cerr << "ERROR: Failed to apply move - " << apply_result.error_message << endl;
            return 1;
        }
        cerr << "Move applied successfully" << endl;
        
        // Validate game state after move
        ValidationResult validation_after = validateGameState(game);
        if (!validation_after.valid) {
            cerr << "ERROR: Game state became invalid - " << validation_after.error_message << endl;
            return 1;
        }
        
        // Output updated game state to both players
        cerr << "\n=== Game State for Player 1 ===" << endl;
        printJsonGameState(game, 1);
        
        cerr << "\n=== Game State for Player 2 ===" << endl;
        printJsonGameState(game, 2);
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
    } else {
        cout << "WINNER: Player " << (winner + 1) << endl;
        cerr << "Player " << (winner + 1) << " wins!" << endl;
    }
    
    return 0;
}
