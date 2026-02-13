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

// Constants for timing
const double INITIAL_TIME_BANK = 300.0; // 5 minutes initial time bank
const double TIME_INCREMENT = 1.0;

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

    bool operator==(const Tokens& other) const {
        return black == other.black && blue == other.blue && white == other.white &&
               green == other.green && red == other.red && joker == other.joker;
    }

    bool operator!=(const Tokens& other) const {
        return !(*this == other);
    }

    Tokens& operator+=(const Tokens& other) {
        black += other.black;
        blue += other.blue;
        white += other.white;
        green += other.green;
        red += other.red;
        joker += other.joker;
        return *this;
    }

    Tokens& operator-=(const Tokens& other) {
        black -= other.black;
        blue -= other.blue;
        white -= other.white;
        green -= other.green;
        red -= other.red;
        joker -= other.joker;
        return *this;
    }

    int& operator[](const std::string& color) {
        if (color == "black") return black;
        if (color == "blue") return blue;
        if (color == "white") return white;
        if (color == "green") return green;
        if (color == "red") return red;
        if (color == "joker") return joker;
        static int dummy; return dummy; // Should handle error better
    }

    const int& operator[](const std::string& color) const {
        if (color == "black") return black;
        if (color == "blue") return blue;
        if (color == "white") return white;
        if (color == "green") return green;
        if (color == "red") return red;
        if (color == "joker") return joker;
        static const int dummy = 0; return dummy;
    }
};

inline Tokens operator+(Tokens lhs, const Tokens& rhs) {
    lhs += rhs;
    return lhs;
}

inline Tokens operator-(Tokens lhs, const Tokens& rhs) {
    lhs -= rhs;
    return lhs;
}

// Struct for a development card
struct Card {
    int id;
    int level;
    int points;
    std::string color;
    Tokens cost;

    Tokens getEffectiveCost(const Tokens& bonuses) const {
        Tokens effective;
        effective.black = std::max(0, cost.black - bonuses.black);
        effective.blue = std::max(0, cost.blue - bonuses.blue);
        effective.white = std::max(0, cost.white - bonuses.white);
        effective.green = std::max(0, cost.green - bonuses.green);
        effective.red = std::max(0, cost.red - bonuses.red);
        return effective;
    }
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
    double time_bank = INITIAL_TIME_BANK; // Time remaining in seconds
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
    int consecutive_passes = 0;       // Count consecutive PASS moves
    
    // Position tracking for REVEAL command in replay mode
    int last_removed_pos_level1 = -1;  // Last removed position from level1
    int last_removed_pos_level2 = -1;  // Last removed position from level2
    int last_removed_pos_level3 = -1;  // Last removed position from level3

    std::vector<Card>& getFaceup(int level) {
        return (level == 1) ? faceup_level1 : (level == 2) ? faceup_level2 : faceup_level3;
    }

    std::vector<Card>& getDeck(int level) {
        return (level == 1) ? deck_level1 : (level == 2) ? deck_level2 : deck_level3;
    }

    int& getLastRemovedPos(int level) {
        return (level == 1) ? last_removed_pos_level1 : (level == 2) ? last_removed_pos_level2 : last_removed_pos_level3;
    }

    struct CardLocation {
        bool found;
        int level;
        int index;

        CardLocation(bool f = false, int l = 0, int i = -1) 
            : found(f), level(l), index(i) {}
    };

    CardLocation findCardInFaceup(int card_id) const {
        if (card_id <= 0) return {false, 0, -1};
        for (int l = 1; l <= 3; l++) {
            const auto& row = (l == 1) ? faceup_level1 : (l == 2) ? faceup_level2 : faceup_level3;
            for (size_t i = 0; i < row.size(); i++) {
                if (row[i].id == card_id) return {true, l, (int)i};
            }
        }
        return {false, 0, -1};
    }
    
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
    REVEAL_CARD,
    PASS_TURN,
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
    
    // For REVEAL_CARD (replay mode only)
    int faceup_level = -1;  // Which level to reveal to
    Card revealed_card;     // The card revealed
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

// Engine helper functions
std::vector<Move> findAllValidMoves(const GameState& state);
std::string moveToString(const Move& m);
GameState parseJson(const std::string& json, const std::vector<Card>& all_c, const std::vector<Noble>& all_n);
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
