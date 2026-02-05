#ifndef GAME_LOGIC_H
#define GAME_LOGIC_H

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <map>
#include <algorithm>
#include <random>
#include <sstream>
#include <ctime>

// Struct to represent gem/token counts
struct Tokens {
    int black = 0;
    int blue = 0;
    int white = 0;
    int green = 0;
    int red = 0;
    int joker = 0;

    Tokens() = default;
    Tokens(int bl, int bu, int wh, int gr, int re, int jo = 0)
        : black(bl), blue(bu), white(wh), green(gr), red(re), joker(jo) {}

    int total() const {
        return black + blue + white + green + red + joker;
    }
};

// Struct for a development card
struct Card {
    int id;
    int level;
    int points;
    std::string color;
    Tokens cost;
};

// Struct for a noble
struct Noble {
    int id;
    int points;
    Tokens requirements;
};

// Struct for player state
struct Player {
    Tokens tokens;           // Gems in hand
    Tokens bonuses;          // Card bonuses (permanent discounts)
    std::vector<Card> cards;      // Purchased cards
    std::vector<Card> reserved;   // Reserved cards (max 3)
    std::vector<Noble> nobles;    // Acquired nobles
    int points = 0;          // Victory points
};

// Main game state
struct GameState {
    bool replay_mode = false;         // Global flag for setup/replay mode
    Tokens bank;                      // Available gems
    Player players[2];                // Two players
    
    std::vector<Card> deck_level1;         // Level 1 deck
    std::vector<Card> deck_level2;         // Level 2 deck
    std::vector<Card> deck_level3;         // Level 3 deck
    
    std::vector<Card> faceup_level1;       // 4 visible level 1 cards
    std::vector<Card> faceup_level2;       // 4 visible level 2 cards
    std::vector<Card> faceup_level3;       // 4 visible level 3 cards
    
    std::vector<Noble> available_nobles;   // 3 nobles in play
    
    int current_player = 0;           // Current player (0 or 1)
    int move_number = 0;              // Move counter
    
    // Position tracking for REVEAL command in replay mode
    int last_removed_pos_level1 = -1;  // Last removed position from level1
    int last_removed_pos_level2 = -1;  // Last removed position from level2
    int last_removed_pos_level3 = -1;  // Last removed position from level3
    
    // Track blind reserve (91/92/93) in replay mode
    int pending_blind_reserve_player = -1;  // Which player has pending blind reserve (-1 = none)
    int pending_blind_reserve_level = -1;   // Which level (1/2/3)
    
    // Track if REVEAL is expected in replay mode
    bool reveal_expected = false;
};

// Enum for move types
enum MoveType {
    TAKE_GEMS,
    RESERVE_CARD,
    BUY_CARD,
    INVALID_MOVE
};

// Structure to represent a player move
struct Move {
    MoveType type = INVALID_MOVE;
    int player_id;  // 0 or 1
    
    // For TAKE_GEMS
    Tokens gems_taken;
    Tokens gems_returned;  // If player exceeds 10 gems
    
    // For RESERVE_CARD or BUY_CARD
    int card_id;  // 1-90 for specific card, 91/92/93 for blind reserve from level 1/2/3
    
    // Payment (for BUY_CARD)
    Tokens payment;
    bool auto_payment = false;  // True if payment should be auto-calculated
    
    // Noble selection (for BUY_CARD when multiple nobles qualify)
    int noble_id = -1;  // -1 means no explicit noble selection
};

// Validation result
struct ValidationResult {
    bool valid;
    std::string error_message;
    
    ValidationResult(bool v = true, std::string msg = "") 
        : valid(v), error_message(msg) {}
};

// Function declarations
ValidationResult validateGameState(const GameState& state);
ValidationResult validateMove(const GameState& state, const Move& move);
ValidationResult validateTakeGems(const GameState& state, const Move& move);
ValidationResult validateReserveCard(const GameState& state, const Move& move);
ValidationResult validateBuyCard(const GameState& state, const Move& move);

std::pair<Move, ValidationResult> parseMove(const std::string& move_string, int player_id);
ValidationResult applyMove(GameState& state, const Move& move, std::ostream& err_os = std::cerr);
void checkAndAssignNobles(GameState& state, int player_idx, int noble_id = -1, std::ostream& err_os = std::cerr);
bool isGameOver(const GameState& state);
int determineWinner(const GameState& state);

Tokens parseTokens(const std::string& json_section);
Tokens calculateAutoPayment(const Tokens& effective_cost, const Tokens& player_tokens);
Card loadCardById(int card_id, const std::vector<Card>& all_cards);
std::vector<Card> loadCards(const std::string& filename, std::ostream& err_os = std::cerr);
std::vector<Noble> loadNobles(const std::string& filename, std::ostream& err_os = std::cerr);

void initializeGame(GameState& state, unsigned int seed = 0, 
                    const std::string& cards_path = "cards.json", 
                    const std::string& nobles_path = "nobles.json", 
                    std::ostream& err_os = std::cerr);
void printGameState(const GameState& state, std::ostream& os = std::cout);

std::string tokensToJson(const Tokens& tokens);
std::string discountsToJson(const Tokens& tokens);
std::string playerToJson(const Player& player, int player_id, int viewer_id);
std::string gameStateToJson(const GameState& state, int viewer_id);
void printJsonGameState(const GameState& state, int viewer_id = 1, std::ostream& os = std::cout);

void processSetupCommands(GameState& state, std::vector<Card>& all_cards, std::vector<Noble>& all_nobles, std::istream& is = std::cin, std::ostream& err_os = std::cerr);
bool processRevealCommand(GameState& state, const std::string& line, std::vector<Card>& all_cards, std::ostream& err_os = std::cerr);


#endif // GAME_LOGIC_H
