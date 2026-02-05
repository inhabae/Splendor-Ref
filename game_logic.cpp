#include "game_logic.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <map>
#include <algorithm>
#include <random>
#include <sstream>
#include <ctime>

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


// Global flag for setup/replay mode
// MOVED TO GameState: bool state.replay_mode = false;

// Game state validation - checks if current state is valid
ValidationResult validateGameState(const GameState& state) {
    // 1. Check total gem count (25 total: 4 of each color + 5 joker)
    int total_gems = state.bank.total() + state.players[0].tokens.total() + state.players[1].tokens.total();
    
    if (total_gems != 25) {
        return ValidationResult(false, "Total gem count is " + to_string(total_gems) + ", expected 25");
    }
    
    // Check individual color totals
    if (state.bank.black + state.players[0].tokens.black + state.players[1].tokens.black != 4) {
        return ValidationResult(false, "Black gem count incorrect");
    }
    if (state.bank.blue + state.players[0].tokens.blue + state.players[1].tokens.blue != 4) {
        return ValidationResult(false, "Blue gem count incorrect");
    }
    if (state.bank.white + state.players[0].tokens.white + state.players[1].tokens.white != 4) {
        return ValidationResult(false, "White gem count incorrect");
    }
    if (state.bank.green + state.players[0].tokens.green + state.players[1].tokens.green != 4) {
        return ValidationResult(false, "Green gem count incorrect");
    }
    if (state.bank.red + state.players[0].tokens.red + state.players[1].tokens.red != 4) {
        return ValidationResult(false, "Red gem count incorrect");
    }
    if (state.bank.joker + state.players[0].tokens.joker + state.players[1].tokens.joker != 5) {
        return ValidationResult(false, "Joker gem count incorrect");
    }
    
    // 2. Check no player has more than 10 gems
    for (int i = 0; i < 2; i++) {
        int player_gems = state.players[i].tokens.total();
        if (player_gems > 10) {
            return ValidationResult(false, "Player " + to_string(i+1) + " has " + 
                                  to_string(player_gems) + " gems (max 10)");
        }
    }
    
    // 3. Check no player has more than 3 reserved cards
    for (int i = 0; i < 2; i++) {
        if (state.players[i].reserved.size() > 3) {
            return ValidationResult(false, "Player " + to_string(i+1) + " has " + 
                                  to_string(state.players[i].reserved.size()) + " reserved cards (max 3)");
        }
    }
    
    // 4. Check all card IDs are unique (no duplicates in play)
    map<int, int> card_count;
    
    // Count face-up cards
    for (const Card& card : state.faceup_level1) card_count[card.id]++;
    for (const Card& card : state.faceup_level2) card_count[card.id]++;
    for (const Card& card : state.faceup_level3) card_count[card.id]++;
    
    // Count deck cards
    for (const Card& card : state.deck_level1) card_count[card.id]++;
    for (const Card& card : state.deck_level2) card_count[card.id]++;
    for (const Card& card : state.deck_level3) card_count[card.id]++;
    
    // Count player cards
    for (int i = 0; i < 2; i++) {
        for (const Card& card : state.players[i].cards) card_count[card.id]++;
        for (const Card& card : state.players[i].reserved) card_count[card.id]++;
    }
    
    // Check for duplicates
    for (const auto& pair : card_count) {
        if (pair.second > 1) {
            return ValidationResult(false, "Card ID " + to_string(pair.first) + 
                                  " appears " + to_string(pair.second) + " times");
        }
    }
    
    // 5. Check player bonuses match their purchased cards
    for (int i = 0; i < 2; i++) {
        Tokens expected_bonuses;
        for (const Card& card : state.players[i].cards) {
            if (card.color == "black") expected_bonuses.black++;
            else if (card.color == "blue") expected_bonuses.blue++;
            else if (card.color == "white") expected_bonuses.white++;
            else if (card.color == "green") expected_bonuses.green++;
            else if (card.color == "red") expected_bonuses.red++;
        }
        
        const Tokens& actual = state.players[i].bonuses;
        if (actual.black != expected_bonuses.black || actual.blue != expected_bonuses.blue ||
            actual.white != expected_bonuses.white || actual.green != expected_bonuses.green ||
            actual.red != expected_bonuses.red || actual.joker != 0) {
            return ValidationResult(false, "Player " + to_string(i+1) + " bonuses don't match purchased cards");
        }
    }
    
    // 6. Check player points match their cards and nobles
    for (int i = 0; i < 2; i++) {
        int expected_points = 0;
        for (const Card& card : state.players[i].cards) {
            expected_points += card.points;
        }
        for (const Noble& noble : state.players[i].nobles) {
            expected_points += noble.points;
        }
        
        if (state.players[i].points != expected_points) {
            return ValidationResult(false, "Player " + to_string(i+1) + " has " + 
                                  to_string(state.players[i].points) + " points, expected " + 
                                  to_string(expected_points));
        }
    }
    
    // 7. Check face-up card positions are valid (4 cards per level)
    if (state.faceup_level1.size() > 4) {
        return ValidationResult(false, "Too many face-up level 1 cards: " + 
                              to_string(state.faceup_level1.size()));
    }
    if (state.faceup_level2.size() > 4) {
        return ValidationResult(false, "Too many face-up level 2 cards: " + 
                              to_string(state.faceup_level2.size()));
    }
    if (state.faceup_level3.size() > 4) {
        return ValidationResult(false, "Too many face-up level 3 cards: " + 
                              to_string(state.faceup_level3.size()));
    }
    
    // 8. Check nobles are valid and not duplicated
    map<int, int> noble_count;
    for (const Noble& noble : state.available_nobles) {
        noble_count[noble.id]++;
    }
    for (int i = 0; i < 2; i++) {
        for (const Noble& noble : state.players[i].nobles) {
            noble_count[noble.id]++;
        }
    }
    
    // Check for duplicates
    for (const auto& pair : noble_count) {
        if (pair.second > 1) {
            return ValidationResult(false, "Noble ID " + to_string(pair.first) + 
                                  " appears " + to_string(pair.second) + " times");
        }
    }
    
    // Check max 3 nobles available
    if (state.available_nobles.size() > 3) {
        return ValidationResult(false, "Too many available nobles: " + 
                              to_string(state.available_nobles.size()));
    }
    
    return ValidationResult(true);
}

// Parse move from single-line string format
pair<Move, ValidationResult> parseMove(const string& move_string, int player_id) {
    Move move;
    move.player_id = player_id;
    
    // Split the move string into tokens
    istringstream iss(move_string);
    vector<string> tokens;
    string token;
    while (iss >> token) {
        tokens.push_back(token);
    }
    
    if (tokens.empty()) {
        return {move, ValidationResult(false, "Empty move string")};
    }
    
    string action = tokens[0];
    
    try {
        if (action == "TAKE") {
            move.type = TAKE_GEMS;
            
            // Find if there's a RETURN clause
            size_t return_idx = tokens.size();
            for (size_t i = 1; i < tokens.size(); i++) {
                if (tokens[i] == "RETURN") {
                    return_idx = i;
                    break;
                }
            }
            
            // Parse gems taken (between TAKE and RETURN/end)
            for (size_t i = 1; i < return_idx; i++) {
                string color = tokens[i];
                if (color == "black") move.gems_taken.black++;
                else if (color == "blue") move.gems_taken.blue++;
                else if (color == "white") move.gems_taken.white++;
                else if (color == "green") move.gems_taken.green++;
                else if (color == "red") move.gems_taken.red++;
                else if (color == "joker") move.gems_taken.joker++;
            }
            
            // Parse gems returned (after RETURN)
            if (return_idx < tokens.size()) {
                for (size_t i = return_idx + 1; i < tokens.size(); i++) {
                    string color = tokens[i];
                    if (color == "black") move.gems_returned.black++;
                    else if (color == "blue") move.gems_returned.blue++;
                    else if (color == "white") move.gems_returned.white++;
                    else if (color == "green") move.gems_returned.green++;
                    else if (color == "red") move.gems_returned.red++;
                    else if (color == "joker") move.gems_returned.joker++;
                }
            }
            
        } else if (action == "RESERVE") {
            move.type = RESERVE_CARD;
            
            if (tokens.size() < 2) {
                return {move, ValidationResult(false, "RESERVE missing card_id")};
            }
            
            move.card_id = std::stoi(tokens[1]);
            
            // Find if there's a RETURN clause
            for (size_t i = 2; i < tokens.size(); i++) {
                if (tokens[i] == "RETURN") {
                    // Parse gems returned (after RETURN)
                    for (size_t j = i + 1; j < tokens.size(); j++) {
                        string color = tokens[j];
                        if (color == "black") move.gems_returned.black++;
                        else if (color == "blue") move.gems_returned.blue++;
                        else if (color == "white") move.gems_returned.white++;
                        else if (color == "green") move.gems_returned.green++;
                        else if (color == "red") move.gems_returned.red++;
                        else if (color == "joker") move.gems_returned.joker++;
                    }
                    break;
                }
            }
            
        } else if (action == "BUY") {
            move.type = BUY_CARD;
            
            if (tokens.size() < 2) {
                return {move, ValidationResult(false, "BUY missing card_id")};
            }
            
            move.card_id = std::stoi(tokens[1]);
            
            // Find USING keyword (optional)
            size_t using_idx = tokens.size();
            for (size_t i = 2; i < tokens.size(); i++) {
                if (tokens[i] == "USING") {
                    using_idx = i;
                    break;
                }
            }
            
            // Find NOBLE keyword (optional, for explicit noble selection)
            size_t noble_idx = tokens.size();
            for (size_t i = 2; i < tokens.size(); i++) {
                if (tokens[i] == "NOBLE") {
                    noble_idx = i;
                    break;
                }
            }
            
            // Check if USING was specified
            if (using_idx < tokens.size()) {
                // USING was specified - parse payment gems
                move.auto_payment = false;
                size_t payment_end = (noble_idx < tokens.size()) ? noble_idx : tokens.size();
                for (size_t i = using_idx + 1; i < payment_end; i++) {
                    string color = tokens[i];
                    if (color == "black") move.payment.black++;
                    else if (color == "blue") move.payment.blue++;
                    else if (color == "white") move.payment.white++;
                    else if (color == "green") move.payment.green++;
                    else if (color == "red") move.payment.red++;
                    else if (color == "joker") move.payment.joker++;
                }
            } else {
                // No USING - auto-calculate payment
                move.auto_payment = true;
            }
            
            // Parse noble selection (after NOBLE)
            if (noble_idx < tokens.size() && noble_idx + 1 < tokens.size()) {
                move.noble_id = std::stoi(tokens[noble_idx + 1]);
            }
            
        } else {
            return {move, ValidationResult(false, "Unknown move action: " + action)};
        }
    } catch (const exception& e) {
        return {move, ValidationResult(false, "Malformed move parameter: " + string(e.what()))};
    }
    
    return {move, ValidationResult(true)};
}


// Validate a move against current game state
ValidationResult validateMove(const GameState& state, const Move& move) {
    int player_idx = move.player_id;
    
    // Check it's the correct player's turn
    if (player_idx != state.current_player) {
        return ValidationResult(false, "Not your turn");
    }
    
    // Dispatch to specific validator based on move type
    switch (move.type) {
        case TAKE_GEMS:
            return validateTakeGems(state, move);
            
        case RESERVE_CARD:
            return validateReserveCard(state, move);
            
        case BUY_CARD:
            return validateBuyCard(state, move);
            
        default:
            return ValidationResult(false, "Invalid move type");
    }
}

// Validate TAKE_GEMS move
ValidationResult validateTakeGems(const GameState& state, const Move& move) {
    const Player& player = state.players[move.player_id];
    const Tokens& taken = move.gems_taken;
    const Tokens& returned = move.gems_returned;
    
    // Cannot take joker gems
    if (taken.joker > 0) {
        return ValidationResult(false, "Cannot take joker gems directly");
    }
    
    int total_taken = taken.total();
    
    // Cannot take 0 gems
    if (total_taken == 0) {
        return ValidationResult(false, "Must take at least 1 gem");
    }
    
    // Count how many different colors are being taken
    int different_colors = 0;
    int max_of_one_color = 0;
    string color_with_max;
    
    if (taken.black > 0) { different_colors++; if (taken.black > max_of_one_color) { max_of_one_color = taken.black; color_with_max = "black"; } }
    if (taken.blue > 0) { different_colors++; if (taken.blue > max_of_one_color) { max_of_one_color = taken.blue; color_with_max = "blue"; } }
    if (taken.white > 0) { different_colors++; if (taken.white > max_of_one_color) { max_of_one_color = taken.white; color_with_max = "white"; } }
    if (taken.green > 0) { different_colors++; if (taken.green > max_of_one_color) { max_of_one_color = taken.green; color_with_max = "green"; } }
    if (taken.red > 0) { different_colors++; if (taken.red > max_of_one_color) { max_of_one_color = taken.red; color_with_max = "red"; } }
    
    // Count how many different colors are available in the bank
    int colors_available_in_bank = 0;
    if (state.bank.black > 0) colors_available_in_bank++;
    if (state.bank.blue > 0) colors_available_in_bank++;
    if (state.bank.white > 0) colors_available_in_bank++;
    if (state.bank.green > 0) colors_available_in_bank++;
    if (state.bank.red > 0) colors_available_in_bank++;

    // Case 1: Taking 2 of the same color
    if (total_taken == 2 && different_colors == 1) {
        // Must have 4+ of that color in bank
        int bank_count = 0;
        if (color_with_max == "black") bank_count = state.bank.black;
        else if (color_with_max == "blue") bank_count = state.bank.blue;
        else if (color_with_max == "white") bank_count = state.bank.white;
        else if (color_with_max == "green") bank_count = state.bank.green;
        else if (color_with_max == "red") bank_count = state.bank.red;
        
        if (bank_count < 4) {
            return ValidationResult(false, "Need 4+ gems in bank to take 2 of same color");
        }
    }
    // Case 2: Taking different colors
    else if (total_taken == different_colors) {
        // Must take min(3, colors_available_in_bank) gems
        int expected_to_take = (colors_available_in_bank < 3) ? colors_available_in_bank : 3;
        if (total_taken != expected_to_take) {
            return ValidationResult(false, "Must take " + to_string(expected_to_take) + " gems when taking different colors (found " + to_string(colors_available_in_bank) + " colors available)");
        }
        
        // Each color must be exactly 1
        if (taken.black > 1 || taken.blue > 1 || taken.white > 1 || 
            taken.green > 1 || taken.red > 1) {
            return ValidationResult(false, "Can only take 1 of each color when taking different colors");
        }
    }
    else {
        return ValidationResult(false, "Invalid gem taking pattern");
    }
    
    // Check that bank has enough of each color
    if (taken.black > state.bank.black) {
        return ValidationResult(false, "Not enough black gems in bank");
    }
    if (taken.blue > state.bank.blue) {
        return ValidationResult(false, "Not enough blue gems in bank");
    }
    if (taken.white > state.bank.white) {
        return ValidationResult(false, "Not enough white gems in bank");
    }
    if (taken.green > state.bank.green) {
        return ValidationResult(false, "Not enough green gems in bank");
    }
    if (taken.red > state.bank.red) {
        return ValidationResult(false, "Not enough red gems in bank");
    }
    
    // Check gem limit after taking and returning
    int player_gems_after = player.tokens.total() + total_taken - returned.total();
    
    // If player would have more than 10 gems after taking, they must return to exactly 10
    if (player.tokens.total() + total_taken > 10) {
        if (player_gems_after != 10) {
            return ValidationResult(false, "Must return gems to have exactly 10 gems");
        }
    } else {
        // If player has 10 or fewer gems after taking, they should not return anything
        if (returned.total() > 0) {
            return ValidationResult(false, "Cannot return gems when you have 10 or fewer gems");
        }
    }
    
    // Check that player has the gems they're trying to return
    // (including gems just taken in this move)
    if (returned.black > player.tokens.black + taken.black) {
        return ValidationResult(false, "Cannot return more black gems than you have");
    }
    if (returned.blue > player.tokens.blue + taken.blue) {
        return ValidationResult(false, "Cannot return more blue gems than you have");
    }
    if (returned.white > player.tokens.white + taken.white) {
        return ValidationResult(false, "Cannot return more white gems than you have");
    }
    if (returned.green > player.tokens.green + taken.green) {
        return ValidationResult(false, "Cannot return more green gems than you have");
    }
    if (returned.red > player.tokens.red + taken.red) {
        return ValidationResult(false, "Cannot return more red gems than you have");
    }
    if (returned.joker > player.tokens.joker) {
        return ValidationResult(false, "Cannot return more joker gems than you have");
    }
    
    return ValidationResult(true);
}

// Validate RESERVE_CARD move
ValidationResult validateReserveCard(const GameState& state, const Move& move) {
    const Player& player = state.players[move.player_id];
    const Tokens& returned = move.gems_returned;
    
    // Check player has less than 3 reserved cards
    if (player.reserved.size() >= 3) {
        return ValidationResult(false, "Player already has 3 reserved cards");
    }
    
    int card_id = move.card_id;
    bool card_found = false;
    
    // Check if card exists
    if (card_id >= 1 && card_id <= 90) {
        // Face-up card - check all face-up positions
        for (const Card& card : state.faceup_level1) {
            if (card.id == card_id) { card_found = true; break; }
        }
        if (!card_found) {
            for (const Card& card : state.faceup_level2) {
                if (card.id == card_id) { card_found = true; break; }
            }
        }
        if (!card_found) {
            for (const Card& card : state.faceup_level3) {
                if (card.id == card_id) { card_found = true; break; }
            }
        }
        
        if (!card_found) {
            return ValidationResult(false, "Card " + to_string(card_id) + " not found on board");
        }
    }
    else if (card_id == 91) {
        // Blind reserve from level 1 deck
        if (state.deck_level1.empty()) {
            return ValidationResult(false, "Level 1 deck is empty");
        }
        card_found = true;
    }
    else if (card_id == 92) {
        // Blind reserve from level 2 deck
        if (state.deck_level2.empty()) {
            return ValidationResult(false, "Level 2 deck is empty");
        }
        card_found = true;
    }
    else if (card_id == 93) {
        // Blind reserve from level 3 deck
        if (state.deck_level3.empty()) {
            return ValidationResult(false, "Level 3 deck is empty");
        }
        card_found = true;
    }
    else {
        return ValidationResult(false, "Invalid card_id: " + to_string(card_id));
    }
    
    // Calculate gems after taking joker
    int joker_gained = (state.bank.joker > 0) ? 1 : 0;
    int player_gems_after = player.tokens.total() + joker_gained - returned.total();
    
    // If player would have more than 10 gems after getting joker, they must return to exactly 10
    if (player.tokens.total() + joker_gained > 10) {
        if (player_gems_after != 10) {
            return ValidationResult(false, "Must return gems to have exactly 10 gems");
        }
    } else {
        // If player has 10 or fewer gems after getting joker, they should not return anything
        if (returned.total() > 0) {
            return ValidationResult(false, "Cannot return gems when you have 10 or fewer gems");
        }
    }
    
    // Check that player has the gems they're trying to return
    if (returned.black > player.tokens.black) {
        return ValidationResult(false, "Cannot return more black gems than you have");
    }
    if (returned.blue > player.tokens.blue) {
        return ValidationResult(false, "Cannot return more blue gems than you have");
    }
    if (returned.white > player.tokens.white) {
        return ValidationResult(false, "Cannot return more white gems than you have");
    }
    if (returned.green > player.tokens.green) {
        return ValidationResult(false, "Cannot return more green gems than you have");
    }
    if (returned.red > player.tokens.red) {
        return ValidationResult(false, "Cannot return more red gems than you have");
    }
    if (returned.joker > player.tokens.joker + joker_gained) {
        return ValidationResult(false, "Cannot return more joker gems than you have");
    }
    
    return ValidationResult(true);
}

// Helper function to calculate automatic payment (minimize joker usage)
Tokens calculateAutoPayment(const Tokens& effective_cost, const Tokens& player_tokens) {
    Tokens payment;
    
    // Pay with exact colors first
    payment.black = min(effective_cost.black, player_tokens.black);
    payment.blue = min(effective_cost.blue, player_tokens.blue);
    payment.white = min(effective_cost.white, player_tokens.white);
    payment.green = min(effective_cost.green, player_tokens.green);
    payment.red = min(effective_cost.red, player_tokens.red);
    
    // Calculate remaining cost and use jokers
    int remaining = 0;
    remaining += effective_cost.black - payment.black;
    remaining += effective_cost.blue - payment.blue;
    remaining += effective_cost.white - payment.white;
    remaining += effective_cost.green - payment.green;
    remaining += effective_cost.red - payment.red;
    
    payment.joker = min(remaining, player_tokens.joker);
    
    return payment;
}

// Validate BUY_CARD move (handles both face-up and reserved cards)
ValidationResult validateBuyCard(const GameState& state, const Move& move) {
    const Player& player = state.players[move.player_id];
    int card_id = move.card_id;
    
    // Find the card
    const Card* target_card = nullptr;
    
    // Check player's reserved cards first
    for (const Card& card : player.reserved) {
        if (card.id == card_id) {
            target_card = &card;
            break;
        }
    }
    
    // If not in reserved, check face-up cards
    if (!target_card) {
        for (const Card& card : state.faceup_level1) {
            if (card.id == card_id) { target_card = &card; break; }
        }
    }
    if (!target_card) {
        for (const Card& card : state.faceup_level2) {
            if (card.id == card_id) { target_card = &card; break; }
        }
    }
    if (!target_card) {
        for (const Card& card : state.faceup_level3) {
            if (card.id == card_id) { target_card = &card; break; }
        }
    }
    
    if (!target_card) {
        return ValidationResult(false, "Card " + to_string(card_id) + " not found");
    }
    
    // Calculate effective cost (card cost - player bonuses)
    Tokens effective_cost;
    effective_cost.black = max(0, target_card->cost.black - player.bonuses.black);
    effective_cost.blue = max(0, target_card->cost.blue - player.bonuses.blue);
    effective_cost.white = max(0, target_card->cost.white - player.bonuses.white);
    effective_cost.green = max(0, target_card->cost.green - player.bonuses.green);
    effective_cost.red = max(0, target_card->cost.red - player.bonuses.red);
    
    // Use payment from move, or calculate if auto_payment is true
    Tokens payment = move.payment;
    if (move.auto_payment) {
        payment = calculateAutoPayment(effective_cost, player.tokens);
    }
    
    // Check player has the gems they're paying with
    if (payment.black > player.tokens.black) {
        return ValidationResult(false, "Not enough black gems");
    }
    if (payment.blue > player.tokens.blue) {
        return ValidationResult(false, "Not enough blue gems");
    }
    if (payment.white > player.tokens.white) {
        return ValidationResult(false, "Not enough white gems");
    }
    if (payment.green > player.tokens.green) {
        return ValidationResult(false, "Not enough green gems");
    }
    if (payment.red > player.tokens.red) {
        return ValidationResult(false, "Not enough red gems");
    }
    if (payment.joker > player.tokens.joker) {
        return ValidationResult(false, "Not enough joker gems");
    }
    
    // Validate payment covers cost
    // For each color, payment must cover effective cost (with jokers as wildcard)
    int jokers_used = 0;
    
    // Black
    if (payment.black < effective_cost.black) {
        jokers_used += effective_cost.black - payment.black;
    } else if (payment.black > effective_cost.black) {
        return ValidationResult(false, "Overpaying black gems");
    }
    
    // Blue
    if (payment.blue < effective_cost.blue) {
        jokers_used += effective_cost.blue - payment.blue;
    } else if (payment.blue > effective_cost.blue) {
        return ValidationResult(false, "Overpaying blue gems");
    }
    
    // White
    if (payment.white < effective_cost.white) {
        jokers_used += effective_cost.white - payment.white;
    } else if (payment.white > effective_cost.white) {
        return ValidationResult(false, "Overpaying white gems");
    }
    
    // Green
    if (payment.green < effective_cost.green) {
        jokers_used += effective_cost.green - payment.green;
    } else if (payment.green > effective_cost.green) {
        return ValidationResult(false, "Overpaying green gems");
    }
    
    // Red
    if (payment.red < effective_cost.red) {
        jokers_used += effective_cost.red - payment.red;
    } else if (payment.red > effective_cost.red) {
        return ValidationResult(false, "Overpaying red gems");
    }
    
    // Check joker usage
    if (jokers_used > payment.joker) {
        return ValidationResult(false, "Not enough jokers to cover cost");
    }
    if (payment.joker > jokers_used) {
        return ValidationResult(false, "Using too many jokers");
    }
    
    // Check noble selection
    // Count how many nobles the player will qualify for after this purchase
    Tokens new_bonuses = player.bonuses;
    if (target_card->color == "black") new_bonuses.black++;
    else if (target_card->color == "blue") new_bonuses.blue++;
    else if (target_card->color == "white") new_bonuses.white++;
    else if (target_card->color == "green") new_bonuses.green++;
    else if (target_card->color == "red") new_bonuses.red++;
    
    vector<int> qualifying_nobles;
    for (const Noble& noble : state.available_nobles) {
        if (new_bonuses.black >= noble.requirements.black &&
            new_bonuses.blue >= noble.requirements.blue &&
            new_bonuses.white >= noble.requirements.white &&
            new_bonuses.green >= noble.requirements.green &&
            new_bonuses.red >= noble.requirements.red) {
            qualifying_nobles.push_back(noble.id);
        }
    }
    
    // Validate noble selection
    if (qualifying_nobles.size() == 0) {
        // No nobles qualify
        if (move.noble_id != -1) {
            return ValidationResult(false, "No nobles qualify, but noble_id specified");
        }
    }
    else if (qualifying_nobles.size() == 1) {
        // One noble qualifies - can be automatic or explicit
        if (move.noble_id != -1 && move.noble_id != qualifying_nobles[0]) {
            return ValidationResult(false, "Noble_id doesn't match the qualifying noble");
        }
    }
    else {
        // Multiple nobles qualify
        // If noble_id specified, check it's valid
        if (move.noble_id != -1) {
            // Check the specified noble is in the qualifying list
            bool found = false;
            for (int noble_id : qualifying_nobles) {
                if (noble_id == move.noble_id) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                return ValidationResult(false, "Specified noble does not qualify");
            }
        }
        // If noble_id not specified, one will be randomly assigned
    }
    
    return ValidationResult(true);
}

// Helper function to load a specific card by ID
Card loadCardById(int card_id, const vector<Card>& all_cards) {
    for (const Card& card : all_cards) {
        if (card.id == card_id) {
            return card;
        }
    }
    // Return empty card if not found
    Card empty_card = {0, 0, 0, "", {}};
    return empty_card;
}

// Apply a validated move to game state
ValidationResult applyMove(GameState& state, const Move& move, ostream& err_os) {
    int player_idx = move.player_id;
    Player& player = state.players[player_idx];
    
    switch (move.type) {
        case TAKE_GEMS:
            // Add gems to player, remove from bank
            player.tokens.black += move.gems_taken.black;
            player.tokens.blue += move.gems_taken.blue;
            player.tokens.white += move.gems_taken.white;
            player.tokens.green += move.gems_taken.green;
            player.tokens.red += move.gems_taken.red;
            
            state.bank.black -= move.gems_taken.black;
            state.bank.blue -= move.gems_taken.blue;
            state.bank.white -= move.gems_taken.white;
            state.bank.green -= move.gems_taken.green;
            state.bank.red -= move.gems_taken.red;
            
            // Return gems if any
            player.tokens.black -= move.gems_returned.black;
            player.tokens.blue -= move.gems_returned.blue;
            player.tokens.white -= move.gems_returned.white;
            player.tokens.green -= move.gems_returned.green;
            player.tokens.red -= move.gems_returned.red;
            player.tokens.joker -= move.gems_returned.joker;
            
            state.bank.black += move.gems_returned.black;
            state.bank.blue += move.gems_returned.blue;
            state.bank.white += move.gems_returned.white;
            state.bank.green += move.gems_returned.green;
            state.bank.red += move.gems_returned.red;
            state.bank.joker += move.gems_returned.joker;
            break;
            
        case RESERVE_CARD: {
            Card card_to_reserve;
            bool found = false;
            
            // Determine which card to reserve
            if (move.card_id >= 1 && move.card_id <= 90) {
                // Face-up card - find and remove it
                for (size_t i = 0; i < state.faceup_level1.size(); i++) {
                    if (state.faceup_level1[i].id == move.card_id) {
                        card_to_reserve = state.faceup_level1[i];
                        state.last_removed_pos_level1 = i;  // Track position for REVEAL
                        state.faceup_level1.erase(state.faceup_level1.begin() + i);
                        
                        // Replace with deck card if available (unless in replay mode)
                        if (!state.replay_mode && !state.deck_level1.empty()) {
                            state.faceup_level1.insert(state.faceup_level1.begin() + i, state.deck_level1.back());
                            state.deck_level1.pop_back();
                        } else if (state.replay_mode && !state.deck_level1.empty()) {
                            state.reveal_expected = true;
                            err_os << "\n>>> PROMPT: Please REVEAL a new level1 card <<<" << endl;
                        }
                        found = true;
                        break;
                    }
                }
                
                if (!found) {
                    for (size_t i = 0; i < state.faceup_level2.size(); i++) {
                        if (state.faceup_level2[i].id == move.card_id) {
                            card_to_reserve = state.faceup_level2[i];
                            state.last_removed_pos_level2 = i;  // Track position for REVEAL
                            state.faceup_level2.erase(state.faceup_level2.begin() + i);
                            
                            if (!state.replay_mode && !state.deck_level2.empty()) {
                                state.faceup_level2.insert(state.faceup_level2.begin() + i, state.deck_level2.back());
                                state.deck_level2.pop_back();
                            } else if (state.replay_mode && !state.deck_level2.empty()) {
                                state.reveal_expected = true;
                                err_os << "\n>>> PROMPT: Please REVEAL a new level2 card <<<" << endl;
                            }
                            found = true;
                            break;
                        }
                    }
                }
                
                if (!found) {
                    for (size_t i = 0; i < state.faceup_level3.size(); i++) {
                        if (state.faceup_level3[i].id == move.card_id) {
                            card_to_reserve = state.faceup_level3[i];
                            state.last_removed_pos_level3 = i;  // Track position for REVEAL
                            state.faceup_level3.erase(state.faceup_level3.begin() + i);
                            
                            if (!state.replay_mode && !state.deck_level3.empty()) {
                                state.faceup_level3.insert(state.faceup_level3.begin() + i, state.deck_level3.back());
                                state.deck_level3.pop_back();
                            } else if (state.replay_mode && !state.deck_level3.empty()) {
                                state.reveal_expected = true;
                                err_os << "\n>>> PROMPT: Please REVEAL a new level3 card <<<" << endl;
                            }
                            found = true;
                            break;
                        }
                    }
                }
            }
            else if (move.card_id == 91 && !state.deck_level1.empty()) {
                if (!state.replay_mode) {
                    // In normal mode, take the actual card from deck
                    card_to_reserve = state.deck_level1.back();
                    state.deck_level1.pop_back();
                } else {
                    // In replay mode, create a placeholder - actual card will come from REVEAL
                    card_to_reserve.id = 91;  // Temporary placeholder ID
                    card_to_reserve.level = 1;
                    state.pending_blind_reserve_player = player_idx;
                    state.pending_blind_reserve_level = 1;
                    state.reveal_expected = true;
                    err_os << "\n>>> PROMPT: Please REVEAL the reserved level1 card <<<" << endl;
                }
                found = true;
            }
            else if (move.card_id == 92 && !state.deck_level2.empty()) {
                if (!state.replay_mode) {
                    card_to_reserve = state.deck_level2.back();
                    state.deck_level2.pop_back();
                } else {
                    card_to_reserve.id = 92;  // Temporary placeholder ID
                    card_to_reserve.level = 2;
                    state.pending_blind_reserve_player = player_idx;
                    state.pending_blind_reserve_level = 2;
                    state.reveal_expected = true;
                    err_os << "\n>>> PROMPT: Please REVEAL the reserved level2 card <<<" << endl;
                }
                found = true;
            }
            else if (move.card_id == 93 && !state.deck_level3.empty()) {
                if (!state.replay_mode) {
                    card_to_reserve = state.deck_level3.back();
                    state.deck_level3.pop_back();
                } else {
                    card_to_reserve.id = 93;  // Temporary placeholder ID
                    card_to_reserve.level = 3;
                    state.pending_blind_reserve_player = player_idx;
                    state.pending_blind_reserve_level = 3;
                    state.reveal_expected = true;
                    err_os << "\n>>> PROMPT: Please REVEAL the reserved level3 card <<<" << endl;
                }
                found = true;
            }
            
            if (found) {
                player.reserved.push_back(card_to_reserve);
            }
            
            // Give joker if available
            if (state.bank.joker > 0) {
                player.tokens.joker++;
                state.bank.joker--;
            }
            
            // Return gems if any
            player.tokens.black -= move.gems_returned.black;
            player.tokens.blue -= move.gems_returned.blue;
            player.tokens.white -= move.gems_returned.white;
            player.tokens.green -= move.gems_returned.green;
            player.tokens.red -= move.gems_returned.red;
            player.tokens.joker -= move.gems_returned.joker;
            
            state.bank.black += move.gems_returned.black;
            state.bank.blue += move.gems_returned.blue;
            state.bank.white += move.gems_returned.white;
            state.bank.green += move.gems_returned.green;
            state.bank.red += move.gems_returned.red;
            state.bank.joker += move.gems_returned.joker;
            break;
        }
            
        case BUY_CARD: {
            Card* target_card = nullptr;
            bool is_reserved = false;
            int reserved_idx = -1;
            
            // Find the card in reserved first
            for (size_t i = 0; i < player.reserved.size(); i++) {
                if (player.reserved[i].id == move.card_id) {
                    target_card = &player.reserved[i];
                    is_reserved = true;
                    reserved_idx = i;
                    break;
                }
            }
            
            // If not in reserved, find in face-up
            int faceup_idx = -1;
            int faceup_level = 0;
            if (!target_card) {
                for (size_t i = 0; i < state.faceup_level1.size(); i++) {
                    if (state.faceup_level1[i].id == move.card_id) {
                        target_card = &state.faceup_level1[i];
                        faceup_idx = i;
                        faceup_level = 1;
                        break;
                    }
                }
            }
            if (!target_card) {
                for (size_t i = 0; i < state.faceup_level2.size(); i++) {
                    if (state.faceup_level2[i].id == move.card_id) {
                        target_card = &state.faceup_level2[i];
                        faceup_idx = i;
                        faceup_level = 2;
                        break;
                    }
                }
            }
            if (!target_card) {
                for (size_t i = 0; i < state.faceup_level3.size(); i++) {
                    if (state.faceup_level3[i].id == move.card_id) {
                        target_card = &state.faceup_level3[i];
                        faceup_idx = i;
                        faceup_level = 3;
                        break;
                    }
                }
            }
            
            if (target_card) {
                // Save card info before removing
                Card purchased_card = *target_card;
                
                // Calculate payment if auto_payment was used
                Tokens payment = move.payment;
                if (move.auto_payment) {
                    Tokens effective_cost;
                    effective_cost.black = max(0, purchased_card.cost.black - player.bonuses.black);
                    effective_cost.blue = max(0, purchased_card.cost.blue - player.bonuses.blue);
                    effective_cost.white = max(0, purchased_card.cost.white - player.bonuses.white);
                    effective_cost.green = max(0, purchased_card.cost.green - player.bonuses.green);
                    effective_cost.red = max(0, purchased_card.cost.red - player.bonuses.red);
                    payment = calculateAutoPayment(effective_cost, player.tokens);
                }
                
                // Remove payment from player, add to bank
                player.tokens.black -= payment.black;
                player.tokens.blue -= payment.blue;
                player.tokens.white -= payment.white;
                player.tokens.green -= payment.green;
                player.tokens.red -= payment.red;
                player.tokens.joker -= payment.joker;
                
                state.bank.black += payment.black;
                state.bank.blue += payment.blue;
                state.bank.white += payment.white;
                state.bank.green += payment.green;
                state.bank.red += payment.red;
                state.bank.joker += payment.joker;
                
                // Add card to player's tableau
                player.cards.push_back(purchased_card);
                
                // Update bonuses
                if (purchased_card.color == "black") player.bonuses.black++;
                else if (purchased_card.color == "blue") player.bonuses.blue++;
                else if (purchased_card.color == "white") player.bonuses.white++;
                else if (purchased_card.color == "green") player.bonuses.green++;
                else if (purchased_card.color == "red") player.bonuses.red++;
                
                // Update points
                player.points += purchased_card.points;
                
                // Remove card from its source
                if (is_reserved) {
                    player.reserved.erase(player.reserved.begin() + reserved_idx);
                } else {
                    // Remove from face-up and replace with deck card
                    if (faceup_level == 1) {
                        state.last_removed_pos_level1 = faceup_idx;  // Track position for REVEAL
                        state.faceup_level1.erase(state.faceup_level1.begin() + faceup_idx);
                        if (!state.replay_mode && !state.deck_level1.empty()) {
                            state.faceup_level1.insert(state.faceup_level1.begin() + faceup_idx, state.deck_level1.back());
                            state.deck_level1.pop_back();
                        } else if (state.replay_mode && !state.deck_level1.empty()) {
                            state.reveal_expected = true;
                            err_os << "\n>>> PROMPT: Please REVEAL a new level1 card <<<" << endl;
                        }
                    } else if (faceup_level == 2) {
                        state.last_removed_pos_level2 = faceup_idx;  // Track position for REVEAL
                        state.faceup_level2.erase(state.faceup_level2.begin() + faceup_idx);
                        if (!state.replay_mode && !state.deck_level2.empty()) {
                            state.faceup_level2.insert(state.faceup_level2.begin() + faceup_idx, state.deck_level2.back());
                            state.deck_level2.pop_back();
                        } else if (state.replay_mode && !state.deck_level2.empty()) {
                            state.reveal_expected = true;
                            err_os << "\n>>> PROMPT: Please REVEAL a new level2 card <<<" << endl;
                        }
                    } else if (faceup_level == 3) {
                        state.last_removed_pos_level3 = faceup_idx;  // Track position for REVEAL
                        state.faceup_level3.erase(state.faceup_level3.begin() + faceup_idx);
                        if (!state.replay_mode && !state.deck_level3.empty()) {
                            state.faceup_level3.insert(state.faceup_level3.begin() + faceup_idx, state.deck_level3.back());
                            state.deck_level3.pop_back();
                        } else if (state.replay_mode && !state.deck_level3.empty()) {
                            state.reveal_expected = true;
                            err_os << "\n>>> PROMPT: Please REVEAL a new level3 card <<<" << endl;
                        }
                    }
                }
                
                checkAndAssignNobles(state, player_idx, move.noble_id, err_os);
            } else {
                return ValidationResult(false, "Card ID " + to_string(move.card_id) + " not found in board or reserved");
            }
            break;
        }
        case INVALID_MOVE:
            return ValidationResult(false, "Attempted to apply an invalid move");
    }
    
    // Switch to next player
    state.current_player = 1 - state.current_player;
    state.move_number++;
    
    return ValidationResult(true);
}

// Check if player qualifies for any nobles and assign them
void checkAndAssignNobles(GameState& state, int player_idx, int noble_id, ostream& err_os) {
    Player& player = state.players[player_idx];
    
    // Find which nobles the player qualifies for
    vector<int> qualifying_noble_indices;
    for (size_t i = 0; i < state.available_nobles.size(); i++) {
        const Noble& noble = state.available_nobles[i];
        if (player.bonuses.black >= noble.requirements.black &&
            player.bonuses.blue >= noble.requirements.blue &&
            player.bonuses.white >= noble.requirements.white &&
            player.bonuses.green >= noble.requirements.green &&
            player.bonuses.red >= noble.requirements.red) {
            qualifying_noble_indices.push_back(i);
        }
    }
    
    // Assign noble based on qualification
    if (qualifying_noble_indices.empty()) {
        // No nobles qualify, nothing to do
        return;
    }
    else if (qualifying_noble_indices.size() == 1) {
        // One noble qualifies - assign automatically
        int idx = qualifying_noble_indices[0];
        player.nobles.push_back(state.available_nobles[idx]);
        player.points += state.available_nobles[idx].points;
        state.available_nobles.erase(state.available_nobles.begin() + idx);
    }
    else {
        // Multiple nobles qualify
        if (noble_id == -1) {
            // No specific noble requested - randomly pick one
            random_device rd;
            mt19937 gen(rd());
            uniform_int_distribution<> dis(0, qualifying_noble_indices.size() - 1);
            int random_choice = dis(gen);
            int idx = qualifying_noble_indices[random_choice];
            
            err_os << "Multiple nobles qualify, randomly assigning noble " 
                 << state.available_nobles[idx].id << endl;
            
            player.nobles.push_back(state.available_nobles[idx]);
            player.points += state.available_nobles[idx].points;
            state.available_nobles.erase(state.available_nobles.begin() + idx);
        }
        else {
            // Find the specified noble
            for (size_t i = 0; i < state.available_nobles.size(); i++) {
                if (state.available_nobles[i].id == noble_id) {
                    player.nobles.push_back(state.available_nobles[i]);
                    player.points += state.available_nobles[i].points;
                    state.available_nobles.erase(state.available_nobles.begin() + i);
                    break;
                }
            }
        }
    }
}

// Check if game has ended
bool isGameOver(const GameState& state) {
    // Game ends when a player reaches 15+ points
    // Rules:
    // - If first player (player 0) reaches 15, second player gets one last turn
    // - If second player (player 1) reaches 15, game ends immediately
    
    bool player0_has_15 = state.players[0].points >= 15;
    bool player1_has_15 = state.players[1].points >= 15;
    
    // No one has reached 15 yet - game continues
    if (!player0_has_15 && !player1_has_15) {
        return false;
    }
    
    // Only player 1 (second player) has 15+ - game ends immediately
    // They were second to move, so no last turn for opponent
    if (player1_has_15 && !player0_has_15) {
        return true;
    }
    
    // Only player 0 (first player) has 15+
    // Player 1 gets one last turn
    if (player0_has_15 && !player1_has_15) {
        // After applyMove, current_player has switched to next player
        // If current_player == 0, that means player 1 just finished their last turn
        // If current_player == 1, player 1 is about to take their last turn
        return state.current_player == 0;
    }
    
    // Both players have 15+ - game is over
    // (This happens when player 1 reaches 15 on their last turn after player 0 reached 15)
    return true;
}

// Determine winner after game ends
// Returns: 0 for player 0 wins, 1 for player 1 wins, -1 for tie
int determineWinner(const GameState& state) {
    const Player& p0 = state.players[0];
    const Player& p1 = state.players[1];
    
    // First tiebreaker: higher points
    if (p0.points > p1.points) {
        return 0;
    } else if (p1.points > p0.points) {
        return 1;
    }
    
    // Second tiebreaker: fewer purchased cards
    int p0_cards = p0.cards.size();
    int p1_cards = p1.cards.size();
    
    if (p0_cards < p1_cards) {
        return 0;
    } else if (p1_cards < p0_cards) {
        return 1;
    }
    
    // Tie game
    return -1;
}

// Simple JSON parsing helper for tokens/costs
Tokens parseTokens(const string& json_section) {
    Tokens tokens;
    
    // Parse format: "cost": {"blue": 1, "green": 1, ...}
    string search_colors[] = {"black", "blue", "white", "green", "red", "joker"};
    int* token_fields[] = {&tokens.black, &tokens.blue, &tokens.white, 
                           &tokens.green, &tokens.red, &tokens.joker};
    
    for (int i = 0; i < 6; i++) {
        size_t color_pos = json_section.find("\"" + search_colors[i] + "\"");
        if (color_pos != string::npos) {
            size_t colon_pos = json_section.find(":", color_pos);
            if (colon_pos != string::npos) {
                size_t num_start = colon_pos + 1;
                size_t num_end = json_section.find_first_of(",}", num_start);
                string num_str = json_section.substr(num_start, num_end - num_start);
                // Trim whitespace
                num_str.erase(0, num_str.find_first_not_of(" \t\n\r"));
                num_str.erase(num_str.find_last_not_of(" \t\n\r") + 1);
                *token_fields[i] = stoi(num_str);
            }
        }
    }
    
    return tokens;
}

// Load cards from cards.json
vector<Card> loadCards(const string& filename, ostream& err_os) {
    vector<Card> cards;
    ifstream file(filename);
    if (!file.is_open()) {
        err_os << "Error: Could not open " << filename << endl;
        return cards;
    }
    
    string line, content;
    while (getline(file, line)) {
        content += line;
    }
    file.close();
    
    // Parse JSON array - look for card objects
    size_t pos = content.find("{");
    while (pos != string::npos) {
        size_t end_pos = content.find("}", pos);
        if (end_pos == string::npos) break;
        
        string card_json = content.substr(pos, end_pos - pos + 1);
        Card card;
        
        // Extract id
        size_t id_pos = card_json.find("\"id\"");
        if (id_pos != string::npos) {
            size_t colon = card_json.find(":", id_pos);
            size_t comma = card_json.find(",", colon);
            card.id = stoi(card_json.substr(colon + 1, comma - colon - 1));
        }
        
        // Extract level
        size_t level_pos = card_json.find("\"level\"");
        if (level_pos != string::npos) {
            size_t colon = card_json.find(":", level_pos);
            size_t comma = card_json.find(",", colon);
            card.level = stoi(card_json.substr(colon + 1, comma - colon - 1));
        }
        
        // Extract points
        size_t points_pos = card_json.find("\"points\"");
        if (points_pos != string::npos) {
            size_t colon = card_json.find(":", points_pos);
            size_t comma = card_json.find(",", colon);
            card.points = stoi(card_json.substr(colon + 1, comma - colon - 1));
        }
        
        // Extract color
        size_t color_pos = card_json.find("\"color\"");
        if (color_pos != string::npos) {
            size_t colon = card_json.find(":", color_pos);
            size_t quote1 = card_json.find("\"", colon);
            size_t quote2 = card_json.find("\"", quote1 + 1);
            card.color = card_json.substr(quote1 + 1, quote2 - quote1 - 1);
        }
        
        // Extract cost
        size_t cost_pos = card_json.find("\"cost\"");
        if (cost_pos != string::npos) {
            size_t brace_start = card_json.find("{", cost_pos);
            size_t brace_end = card_json.find("}", brace_start);
            string cost_section = card_json.substr(brace_start, brace_end - brace_start + 1);
            card.cost = parseTokens(cost_section);
        }
        
        cards.push_back(card);
        
        pos = content.find("{", end_pos);
    }
    
    return cards;
}

// Load nobles from nobles.json
vector<Noble> loadNobles(const string& filename, ostream& err_os) {
    vector<Noble> nobles;
    ifstream file(filename);
    if (!file.is_open()) {
        err_os << "Error: Could not open " << filename << endl;
        return nobles;
    }
    
    string line, content;
    while (getline(file, line)) {
        content += line;
    }
    file.close();
    
    // Parse JSON array - look for noble objects
    size_t pos = content.find("{");
    while (pos != string::npos) {
        size_t end_pos = content.find("}", pos);
        if (end_pos == string::npos) break;
        
        string noble_json = content.substr(pos, end_pos - pos + 1);
        Noble noble;
        
        // Extract id
        size_t id_pos = noble_json.find("\"id\"");
        if (id_pos != string::npos) {
            size_t colon = noble_json.find(":", id_pos);
            size_t comma = noble_json.find(",", colon);
            noble.id = stoi(noble_json.substr(colon + 1, comma - colon - 1));
        }
        
        // Extract points
        size_t points_pos = noble_json.find("\"points\"");
        if (points_pos != string::npos) {
            size_t colon = noble_json.find(":", points_pos);
            size_t comma = noble_json.find(",", colon);
            noble.points = stoi(noble_json.substr(colon + 1, comma - colon - 1));
        }
        
        // Extract requirements
        size_t req_pos = noble_json.find("\"requirements\"");
        if (req_pos != string::npos) {
            size_t brace_start = noble_json.find("{", req_pos);
            size_t brace_end = noble_json.find("}", brace_start);
            string req_section = noble_json.substr(brace_start, brace_end - brace_start + 1);
            noble.requirements = parseTokens(req_section);
        }
        
        nobles.push_back(noble);
        
        pos = content.find("{", end_pos);
    }
    
    return nobles;
}

// Initialize game state
void initializeGame(GameState& state, unsigned int seed, 
                    const string& cards_path, 
                    const string& nobles_path, 
                    ostream& err_os) {
    // Use provided seed or current time
    if (seed == 0) {
        seed = static_cast<unsigned int>(time(nullptr));
    }
    mt19937 rng(seed);
    
    err_os << "Initializing game with seed: " << seed << endl;
    
    // Load all cards
    vector<Card> all_cards = loadCards(cards_path, err_os);
    err_os << "Loaded " << all_cards.size() << " cards" << endl;
    
    // Separate cards by level
    vector<Card> level1, level2, level3;
    for (const Card& card : all_cards) {
        if (card.level == 1) level1.push_back(card);
        else if (card.level == 2) level2.push_back(card);
        else if (card.level == 3) level3.push_back(card);
    }
    
    err_os << "Level 1: " << level1.size() << " cards" << endl;
    err_os << "Level 2: " << level2.size() << " cards" << endl;
    err_os << "Level 3: " << level3.size() << " cards" << endl;
    
    // Shuffle each level
    shuffle(level1.begin(), level1.end(), rng);
    shuffle(level2.begin(), level2.end(), rng);
    shuffle(level3.begin(), level3.end(), rng);
    
    // Draw 4 cards from each level for face-up display
    for (int i = 0; i < 4 && i < (int)level1.size(); i++) {
        state.faceup_level1.push_back(level1[i]);
    }
    for (int i = 4; i < (int)level1.size(); i++) {
        state.deck_level1.push_back(level1[i]);
    }
    
    for (int i = 0; i < 4 && i < (int)level2.size(); i++) {
        state.faceup_level2.push_back(level2[i]);
    }
    for (int i = 4; i < (int)level2.size(); i++) {
        state.deck_level2.push_back(level2[i]);
    }
    
    for (int i = 0; i < 4 && i < (int)level3.size(); i++) {
        state.faceup_level3.push_back(level3[i]);
    }
    for (int i = 4; i < (int)level3.size(); i++) {
        state.deck_level3.push_back(level3[i]);
    }
    
    err_os << "Face-up cards drawn: " << state.faceup_level1.size() 
         << " (L1), " << state.faceup_level2.size() 
         << " (L2), " << state.faceup_level3.size() << " (L3)" << endl;
    
    // Load and shuffle nobles
    vector<Noble> all_nobles = loadNobles(nobles_path, err_os);
    err_os << "Loaded " << all_nobles.size() << " nobles" << endl;
    
    shuffle(all_nobles.begin(), all_nobles.end(), rng);
    
    // Draw 3 nobles
    for (int i = 0; i < 3 && i < (int)all_nobles.size(); i++) {
        state.available_nobles.push_back(all_nobles[i]);
    }
    
    err_os << "Nobles in play: " << state.available_nobles.size() << endl;
    
    // Initialize bank gems (4 of each color, 5 joker)
    state.bank.black = 4;
    state.bank.blue = 4;
    state.bank.white = 4;
    state.bank.green = 4;
    state.bank.red = 4;
    state.bank.joker = 5;
    
    err_os << "Bank initialized: " << state.bank.total() << " total gems" << endl;
    
    // Initialize player gems to 0 (already default initialized)
    err_os << "Players initialized with 0 gems" << endl;
    
    state.current_player = 0;
    state.move_number = 0;
    
    err_os << "Game initialization complete!" << endl;
}

// Helper function to print game state (for testing)
void printGameState(const GameState& state, ostream& os) {
    os << "\n=== GAME STATE ===" << endl;
    os << "Move: " << state.move_number << ", Current Player: " << state.current_player << endl;
    
    os << "\nBank: Black=" << state.bank.black << " Blue=" << state.bank.blue 
         << " White=" << state.bank.white << " Green=" << state.bank.green 
         << " Red=" << state.bank.red << " Joker=" << state.bank.joker << endl;
    
    os << "\nFace-up Level 1 Cards:" << endl;
    for (const Card& card : state.faceup_level1) {
        os << "  Card #" << card.id << ": " << card.color << " (" << card.points << " pts)" << endl;
    }
    
    os << "\nFace-up Level 2 Cards:" << endl;
    for (const Card& card : state.faceup_level2) {
        os << "  Card #" << card.id << ": " << card.color << " (" << card.points << " pts)" << endl;
    }
    
    os << "\nFace-up Level 3 Cards:" << endl;
    for (const Card& card : state.faceup_level3) {
        os << "  Card #" << card.id << ": " << card.color << " (" << card.points << " pts)" << endl;
    }
    
    os << "\nNobles:" << endl;
    for (const Noble& noble : state.available_nobles) {
        os << "  Noble #" << noble.id << " (" << noble.points << " pts)" << endl;
    }
    
    os << "\nPlayer 0: " << state.players[0].points << " points, " 
         << state.players[0].tokens.total() << " gems" << endl;
    os << "Player 1: " << state.players[1].points << " points, " 
         << state.players[1].tokens.total() << " gems" << endl;
    
    os << "==================\n" << endl;
}

// Convert Tokens to JSON string (alphabetical order, joker last)
string tokensToJson(const Tokens& tokens) {
    stringstream ss;
    ss << "{";
    ss << "\"black\":" << tokens.black << ",";
    ss << "\"blue\":" << tokens.blue << ",";
    ss << "\"green\":" << tokens.green << ",";
    ss << "\"red\":" << tokens.red << ",";
    ss << "\"white\":" << tokens.white << ",";
    ss << "\"joker\":" << tokens.joker;
    ss << "}";
    return ss.str();
}

// Convert card bonuses/discounts to JSON (alphabetical order, no joker field)
string discountsToJson(const Tokens& tokens) {
    stringstream ss;
    ss << "{";
    ss << "\"black\":" << tokens.black << ",";
    ss << "\"blue\":" << tokens.blue << ",";
    ss << "\"green\":" << tokens.green << ",";
    ss << "\"red\":" << tokens.red << ",";
    ss << "\"white\":" << tokens.white;
    ss << "}";
    return ss.str();
}

// Convert Player to JSON string (spec format)
string playerToJson(const Player& player, int player_id, int viewer_id) {
    stringstream ss;
    ss << "{";
    ss << "\"id\":" << player_id << ",";
    ss << "\"points\":" << player.points << ",";
    ss << "\"gems\":" << tokensToJson(player.tokens) << ",";
    ss << "\"discounts\":" << discountsToJson(player.bonuses) << ",";
    
    // Reserved card IDs (mask if viewing opponent)
    ss << "\"reserved_card_ids\":[";
    for (size_t i = 0; i < player.reserved.size(); i++) {
        if (i > 0) ss << ",";
        // Mask opponent's reserved cards with 91/92/93 based on card level
        // (Don't mask if viewer_id is 0, meaning god mode/spectator)
        if (viewer_id != 0 && player_id != viewer_id) {
            ss << (90 + player.reserved[i].level);
        } else {
            ss << player.reserved[i].id;
        }
    }
    ss << "],";
    
    // Purchased card IDs
    ss << "\"purchased_card_ids\":[";
    for (size_t i = 0; i < player.cards.size(); i++) {
        if (i > 0) ss << ",";
        ss << player.cards[i].id;
    }
    ss << "],";
    
    // Owned noble IDs
    ss << "\"owned_noble_ids\":[";
    for (size_t i = 0; i < player.nobles.size(); i++) {
        if (i > 0) ss << ",";
        ss << player.nobles[i].id;
    }
    ss << "],";
    
    // Time bank (initial 20 seconds per player)
    ss << "\"time_bank\":20.0";
    
    ss << "}";
    return ss.str();
}

// Convert entire GameState to JSON string (spec format)
string gameStateToJson(const GameState& state, int viewer_id) {
    stringstream ss;
    ss << "{";
    
    // Active player (1-indexed)
    ss << "\"active_player_id\":" << (state.current_player + 1) << ",";
    
    // Who is viewing this state (omit if viewer_id is 0)
    if (viewer_id != 0) {
        ss << "\"you\":" << viewer_id << ",";
    }
    
    // Move number (1-indexed)
    ss << "\"move\":" << (state.move_number + 1) << ",";
    
    // Players array
    ss << "\"players\":[";
    ss << playerToJson(state.players[0], 1, viewer_id) << ",";
    ss << playerToJson(state.players[1], 2, viewer_id);
    ss << "],";
    
    // Board object
    ss << "\"board\":{";
    
    // Gems on board
    ss << "\"gems\":" << tokensToJson(state.bank) << ",";
    
    // Face-up cards by level (just IDs)
    ss << "\"face_up_cards\":{";
    
    ss << "\"level1\":[";
    for (size_t i = 0; i < state.faceup_level1.size(); i++) {
        if (i > 0) ss << ",";
        ss << state.faceup_level1[i].id;
    }
    ss << "],";
    
    ss << "\"level2\":[";
    for (size_t i = 0; i < state.faceup_level2.size(); i++) {
        if (i > 0) ss << ",";
        ss << state.faceup_level2[i].id;
    }
    ss << "],";
    
    ss << "\"level3\":[";
    for (size_t i = 0; i < state.faceup_level3.size(); i++) {
        if (i > 0) ss << ",";
        ss << state.faceup_level3[i].id;
    }
    ss << "]";
    
    ss << "},";
    
    // Nobles (just IDs)
    ss << "\"nobles\":[";
    for (size_t i = 0; i < state.available_nobles.size(); i++) {
        if (i > 0) ss << ",";
        ss << state.available_nobles[i].id;
    }
    ss << "]";
    
    ss << "}";  // end board
    
    ss << "}";  // end root
    return ss.str();
}

// Print game state as JSON (for a specific viewer)
void printJsonGameState(const GameState& state, int viewer_id, ostream& os) {
    os << gameStateToJson(state, viewer_id) << endl;
}

// Process SETUP commands for replay mode
void processSetupCommands(GameState& state, vector<Card>& all_cards, vector<Noble>& all_nobles, istream& is, ostream& err_os) {
    string line;
    
    while (getline(is, line)) {
        istringstream iss(line);
        string command;
        iss >> command;
        
        if (command == "BEGIN") {
            // Validate that all required setup is complete
            bool level1_setup = !state.faceup_level1.empty();
            bool level2_setup = !state.faceup_level2.empty();
            bool level3_setup = !state.faceup_level3.empty();
            bool nobles_setup = !state.available_nobles.empty();
            
            if (!level1_setup || !level2_setup || !level3_setup || !nobles_setup) {
                err_os << "ERROR: Cannot BEGIN - incomplete setup!" << endl;
                err_os << "  level1 face-up cards: " << (level1_setup ? "✓" : "✗") << endl;
                err_os << "  level2 face-up cards: " << (level2_setup ? "✓" : "✗") << endl;
                err_os << "  level3 face-up cards: " << (level3_setup ? "✓" : "✗") << endl;
                err_os << "  nobles: " << (nobles_setup ? "✓" : "✗") << endl;
                exit(1);
            }
            
            // Auto-populate decks that weren't manually set up
            if (state.deck_level1.empty()) {
                err_os << "Auto-populating level1 deck with remaining cards..." << endl;
                for (const Card& card : all_cards) {
                    if (card.level == 1) {
                        bool on_board = false;
                        for (const Card& board_card : state.faceup_level1) {
                            if (board_card.id == card.id) {
                                on_board = true;
                                break;
                            }
                        }
                        if (!on_board) {
                            state.deck_level1.push_back(card);
                        }
                    }
                }
            }
            
            if (state.deck_level2.empty()) {
                err_os << "Auto-populating level2 deck with remaining cards..." << endl;
                for (const Card& card : all_cards) {
                    if (card.level == 2) {
                        bool on_board = false;
                        for (const Card& board_card : state.faceup_level2) {
                            if (board_card.id == card.id) {
                                on_board = true;
                                break;
                            }
                        }
                        if (!on_board) {
                            state.deck_level2.push_back(card);
                        }
                    }
                }
            }
            
            if (state.deck_level3.empty()) {
                err_os << "Auto-populating level3 deck with remaining cards..." << endl;
                for (const Card& card : all_cards) {
                    if (card.level == 3) {
                        bool on_board = false;
                        for (const Card& board_card : state.faceup_level3) {
                            if (board_card.id == card.id) {
                                on_board = true;
                                break;
                            }
                        }
                        if (!on_board) {
                            state.deck_level3.push_back(card);
                        }
                    }
                }
            }
            
            err_os << "Setup complete, starting game" << endl;
            break;
        }
        else if (command == "SETUP_FACEUP") {
            string level;
            iss >> level;
            
            int id;
            while (iss >> id) {
                Card card = loadCardById(id, all_cards);
                if (card.id != 0) {
                    if (level == "level1") state.faceup_level1.push_back(card);
                    else if (level == "level2") state.faceup_level2.push_back(card);
                    else if (level == "level3") state.faceup_level3.push_back(card);
                }
            }
        }
        else if (command == "SETUP_NOBLES") {
            int id;
            while (iss >> id) {
                for (const Noble& noble : all_nobles) {
                    if (noble.id == id) {
                        state.available_nobles.push_back(noble);
                        break;
                    }
                }
            }
        }
        else if (command == "SETUP_DECK") {
            string level;
            iss >> level;
            
            vector<int> card_ids;
            int id;
            while (iss >> id) {
                card_ids.push_back(id);
            }
            
            // Add to deck in reverse order (so first card is at back)
            for (auto it = card_ids.rbegin(); it != card_ids.rend(); ++it) {
                Card card = loadCardById(*it, all_cards);
                if (card.id != 0) {
                    if (level == "level1") state.deck_level1.push_back(card);
                    else if (level == "level2") state.deck_level2.push_back(card);
                    else if (level == "level3") state.deck_level3.push_back(card);
                }
            }
        }
    }
}

// Process REVEAL command to manually place a card
bool processRevealCommand(GameState& state, const string& line, vector<Card>& all_cards, ostream& err_os) {
    istringstream iss(line);
    string command;
    int card_id;
    
    iss >> command >> card_id;
    
    if (command != "REVEAL") {
        return false;
    }
    
    Card card = loadCardById(card_id, all_cards);
    if (card.id == 0) {
        err_os << "ERROR: Card " << card_id << " not found" << endl;
        return false;
    }
    
    // Select the appropriate deck based on card's level
    vector<Card>* deck = nullptr;
    vector<Card>* faceup = nullptr;
    
    if (card.level == 1) {
        deck = &state.deck_level1;
        faceup = &state.faceup_level1;
    } else if (card.level == 2) {
        deck = &state.deck_level2;
        faceup = &state.faceup_level2;
    } else if (card.level == 3) {
        deck = &state.deck_level3;
        faceup = &state.faceup_level3;
    } else {
        return false;
    }
    
    // Check if this REVEAL is for a blind reserve (91/92/93)
    if (state.pending_blind_reserve_player >= 0 && state.pending_blind_reserve_level == card.level) {
        int player_idx = state.pending_blind_reserve_player;
        
        bool found_in_deck = false;
        for (auto it = deck->begin(); it != deck->end(); ++it) {
            if (it->id == card_id) {
                deck->erase(it);
                found_in_deck = true;
                break;
            }
        }
        
        if (!found_in_deck) return false;
        
        if (!state.players[player_idx].reserved.empty()) {
            state.players[player_idx].reserved.back() = card;
        }
        
        state.pending_blind_reserve_player = -1;
        state.pending_blind_reserve_level = -1;
        state.reveal_expected = false;
        return true;
    }
    
    // For face-up card replacements
    bool found_in_deck = false;
    for (auto it = deck->begin(); it != deck->end(); ++it) {
        if (it->id == card_id) {
            deck->erase(it);
            found_in_deck = true;
            break;
        }
    }
    
    if (!found_in_deck) return false;
    
    int insert_pos = -1;
    if (card.level == 1 && state.last_removed_pos_level1 >= 0) {
        insert_pos = min(state.last_removed_pos_level1, (int)faceup->size());
        state.last_removed_pos_level1 = -1;
    } else if (card.level == 2 && state.last_removed_pos_level2 >= 0) {
        insert_pos = min(state.last_removed_pos_level2, (int)faceup->size());
        state.last_removed_pos_level2 = -1;
    } else if (card.level == 3 && state.last_removed_pos_level3 >= 0) {
        insert_pos = min(state.last_removed_pos_level3, (int)faceup->size());
        state.last_removed_pos_level3 = -1;
    }
    
    if (insert_pos >= 0) {
        faceup->insert(faceup->begin() + insert_pos, card);
    } else {
        faceup->push_back(card);
    }
    
    state.reveal_expected = false;
    return true;
}
