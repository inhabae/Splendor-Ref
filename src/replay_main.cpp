// Replay - Replay Mode
// This executable runs the replay mode that processes game files and outputs JSON states

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
    game.replay_mode = true;
    
    // Load all cards and nobles for replay mode
    vector<Card> all_cards = loadCards("data/cards.json");
    vector<Noble> all_nobles = loadNobles("data/nobles.json");
    
    cerr << "Loaded " << all_cards.size() << " cards and " << all_nobles.size() << " nobles" << endl;
    cerr << "\n=== REPLAY MODE ENABLED ===" << endl;
    
    // Initialize empty game state
    game.bank.black = 4;
    game.bank.blue = 4;
    game.bank.white = 4;
    game.bank.green = 4;
    game.bank.red = 4;
    game.bank.joker = 5;
    game.current_player = 0;
    game.move_number = 0;
    
    // Process setup commands
    processSetupCommands(game, all_cards, all_nobles);
    
    // Output initial state as move 1
    cerr << "\n=== Initial Game State ===" << endl;
    cout << "[" << endl;
    cout << gameStateToJson(game, 0);
    
    cerr << "\n=== Starting Game Loop ===" << endl;
    
    // Read all commands ahead of time to check for REVEAL
    vector<string> commands;
    string line;
    while (getline(cin, line)) {
        commands.push_back(line);
    }
    
    size_t command_idx = 0;
    while (command_idx < commands.size() && !isGameOver(game)) {
        string move_string = commands[command_idx];
        int current = game.current_player;
        cerr << "\nProcessing command " << (command_idx + 1) << ": \"" << move_string << "\"" << endl;
        
        // Check if this is a REVEAL command (valid in replay mode)
        if (move_string.find("REVEAL") == 0) {
            processRevealCommand(game, move_string, all_cards);
            
            // Output updated game state after reveal (single state)
            cerr << "\n=== Game State (after reveal) ===" << endl;
            cout << "," << endl << gameStateToJson(game, 0);
            
            command_idx++;
            continue; // Don't process as regular move
        }
        
        // Check if next command is REVEAL - if so, we'll process it immediately
        bool next_is_reveal = (command_idx + 1 < commands.size() && 
                               commands[command_idx + 1].find("REVEAL") == 0);
        
        // In replay mode, check if REVEAL is expected but not found
        if (game.reveal_expected && !next_is_reveal) {
            cerr << "ERROR: Expected REVEAL command but received: " << move_string << endl;
            cerr << "Game terminated due to missing REVEAL command" << endl;
            break;
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
            
            // Output result to cerr
            cerr << "WINNER: Player " << (2 - current) << endl;
            cerr << "REASON: Player " << (current + 1) << " made invalid move" << endl;
            break;
        }
        
        // Apply the move
        applyMove(game, move);
        cerr << "Move applied successfully" << endl;
        
        // Validate game state after move
        ValidationResult validation = validateGameState(game);
        if (!validation.valid) {
            cerr << "ERROR: Game state became invalid - " << validation.error_message << endl;
            break;
        }
        
        // If next command is REVEAL, process it before outputting state
        if (next_is_reveal) {
            cerr << "Next command is REVEAL, processing it before output..." << endl;
            command_idx++;
            string reveal_string = commands[command_idx];
            processRevealCommand(game, reveal_string, all_cards);
            
            // Switch player after reveal is processed (since applyMove skipped it)
            game.current_player = 1 - game.current_player;
            game.move_number++;
        }
        
        // Output updated game state (single state, includes REVEAL if it was next)
        cerr << "\n=== Game State ===" << endl;
        cout << "," << endl << gameStateToJson(game, 0);
        
        command_idx++;
    }
    
    cout << endl << "]" << endl;
    
    // Game ended - determine winner
    cerr << "\n=== Game Over ===" << endl;
    int winner = determineWinner(game);
    
    cerr << "Final Scores:" << endl;
    cerr << "  Player 1: " << game.players[0].points << " points, " 
         << game.players[0].cards.size() << " cards" << endl;
    cerr << "  Player 2: " << game.players[1].points << " points, " 
         << game.players[1].cards.size() << " cards" << endl;
    
    // Output winner to cerr
    if (winner == -1) {
        cerr << "Game ended in a tie" << endl;
    } else {
        cerr << "Player " << (winner + 1) << " wins!" << endl;
    }
    
    return 0;
}
