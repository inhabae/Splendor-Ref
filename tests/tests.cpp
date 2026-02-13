#define CATCH_CONFIG_MAIN
#include "catch.hpp"
#include "game_logic.h"
#include <vector>
#include <string>
#include <iostream>

using namespace std;

// Helper to silence cerr during test setup to keep output clean
struct SilenceCerr {
    std::streambuf* old;
    SilenceCerr() : old(std::cerr.rdbuf(nullptr)) {}
    ~SilenceCerr() { std::cerr.rdbuf(old); }
};

// Helper to get a card by ID from the game data for manual setup
Card findCardInGame(const GameState& state, int id) {
    for (const auto& c : state.faceup_level1) if (c.id == id) return c;
    for (const auto& c : state.faceup_level2) if (c.id == id) return c;
    for (const auto& c : state.faceup_level3) if (c.id == id) return c;
    for (const auto& c : state.deck_level1) if (c.id == id) return c;
    for (const auto& c : state.deck_level2) if (c.id == id) return c;
    for (const auto& c : state.deck_level3) if (c.id == id) return c;
    return Card();
}

TEST_CASE("G. Row Management and Empty Decks", "[spec]") {
    GameState state;
    {
        SilenceCerr sc;
        initializeGame(state, 123, "data/cards.json", "data/nobles.json");
    }
    
    SECTION("Row shows placeholder when deck is empty") {
        // Setup: Force Level 1 deck to be empty
        state.deck_level1.clear();
        int initial_size = state.faceup_level1.size();
        REQUIRE(initial_size == 4);
        
        // Action: Reserve a card from Level 1
        int card_id = state.faceup_level1[0].id;
        Move move;
        move.type = RESERVE_CARD;
        move.player_id = state.current_player;
        move.card_id = card_id;
        
        {
            SilenceCerr sc;
            ValidationResult v = validateMove(state, move);
            REQUIRE(v.valid);
            applyMove(state, move);
        }
        
        // Assert: Size should still be 4 (placeholder inserted)
        CHECK(state.faceup_level1.size() == 4);
        CHECK(state.faceup_level1[0].id == 0); // Placeholder id
        
        // Verify JSON
        string json = gameStateToJson(state, 0); // Spectator view
        // The level1 array in JSON should have exactly 4 IDs, with the first being 0
        // Looking for "level1":[0,ID,ID,ID]
        size_t l1_pos = json.find("\"level1\":[0,");
        CHECK(l1_pos != string::npos);
        
        // Count commas in the array substring to deduce element count
        size_t l1_end = json.find("]", l1_pos);
        string l1_array = json.substr(l1_pos, l1_end - l1_pos + 1);
        int commas = 0;
        for (char c : l1_array) if (c == ',') commas++;
        CHECK(commas == 3); // 4 elements = 3 commas
    }
}

TEST_CASE("F. Nobles and JSON Masking Requirements", "[spec]") {
    GameState state;
    {
        SilenceCerr sc;
        initializeGame(state, 123, "data/cards.json", "data/nobles.json");
    }
    
    SECTION("Invalid Move: TAKE with NOBLE") {
        Move move;
        move.type = TAKE_GEMS;
        move.player_id = 0;
        move.gems_taken.red = 1;
        move.gems_taken.blue = 1;
        move.gems_taken.green = 1;
        move.noble_id = 1; // Attempt to specify a noble
        
        ValidationResult v = validateMove(state, move);
        CHECK_FALSE(v.valid);
        CHECK(v.error_message.find("not specify a noble") != string::npos);
    }
    
    SECTION("Invalid Move: RESERVE with NOBLE") {
        Move move;
        move.type = RESERVE_CARD;
        move.player_id = 0;
        move.card_id = state.faceup_level1[0].id;
        move.noble_id = 1; // Attempt to specify a noble
        
        ValidationResult v = validateMove(state, move);
        CHECK_FALSE(v.valid);
        CHECK(v.error_message.find("not specify a noble") != string::npos);
    }
    
    SECTION("JSON Masking: Opponent Reserved Card IDs") {
        // Setup: Player 1 (viewer) and Player 2 (opponent)
        // Player 2 reserves cards of different levels
        state.players[1].reserved.clear();
        state.players[1].reserved.push_back({10, 1, 0, "red", {}}); // Level 1
        state.players[1].reserved.push_back({50, 2, 0, "blue", {}}); // Level 2
        state.players[1].reserved.push_back({80, 3, 0, "white", {}}); // Level 3
        
        // View as Player 1 (viewer_id = 1)
        string json = playerToJson(state.players[1], 2, 1);
        
        // Assertions: Should contain 91, 92, 93 and NOT the original IDs 10, 50, 80
        CHECK(json.find("\"reserved_card_ids\":[91,92,93]") != string::npos);
        
        // Test Case: Two Level 2 cards should show as [92, 92]
        state.players[1].reserved.clear();
        state.players[1].reserved.push_back({51, 2, 0, "green", {}}); 
        state.players[1].reserved.push_back({52, 2, 0, "red", {}});
        
        string json_double_l2 = playerToJson(state.players[1], 2, 1);
        CHECK(json_double_l2.find("\"reserved_card_ids\":[92,92]") != string::npos);
        
        // View as Player 2 (viewer_id = 2) - should see original IDs
        string json_self = playerToJson(state.players[1], 2, 2);
        CHECK(json_self.find("\"reserved_card_ids\":[51,52]") != string::npos);
    }
}

TEST_CASE("A. Token Management (TAKE moves)", "[tokens]") {
    GameState state;
    {
        SilenceCerr sc;
        state.replay_mode = false;
        initializeGame(state, 123, "data/cards.json", "data/nobles.json");
    }
    int p = state.current_player;

    SECTION("Valid 3-Color Take") {
        Move move;
        move.type = TAKE_GEMS;
        move.player_id = p;
        move.gems_taken.white = 1;
        move.gems_taken.blue = 1;
        move.gems_taken.green = 1;

        ValidationResult v = validateMove(state, move);
        INFO(v.error_message);
        REQUIRE(v.valid);

        applyMove(state, move);
        CHECK(state.players[p].tokens.white == 1);
        CHECK(state.players[p].tokens.blue == 1);
        CHECK(state.players[p].tokens.green == 1);
        CHECK(state.bank.white == 3);
    }

    SECTION("Valid 2-Color Take (Bank >= 4)") {
        state.bank.red = 4;
        Move move;
        move.type = TAKE_GEMS;
        move.player_id = p;
        move.gems_taken.red = 2;

        ValidationResult v = validateMove(state, move);
        INFO(v.error_message);
        REQUIRE(v.valid);

        applyMove(state, move);
        CHECK(state.players[p].tokens.red == 2);
        CHECK(state.bank.red == 2);
    }

    SECTION("Invalid 2-Color Take (Bank < 4)") {
        state.bank.red = 3;
        Move move;
        move.type = TAKE_GEMS;
        move.player_id = p;
        move.gems_taken.red = 2;

        ValidationResult v = validateMove(state, move);
        INFO("Expected error but got valid move");
        CHECK_FALSE(v.valid);
        if (!v.valid) {
            INFO("Actual error message: " << v.error_message);
            CHECK(v.error_message.find("4+ gems") != string::npos);
        }
    }

    SECTION("Bank Exhaustion: Taking 0 tokens") {
        state.bank.white = 0;
        Move move;
        move.type = TAKE_GEMS;
        move.player_id = p;
        move.gems_taken.white = 1;
        move.gems_taken.blue = 1;
        move.gems_taken.green = 1;

        ValidationResult v = validateMove(state, move);
        CHECK_FALSE(v.valid);
    }

    SECTION("The 10-Token Limit (Return excess)") {
        // Player has 9 gems
        state.players[p].tokens.white = 9;
        
        Move move;
        move.type = TAKE_GEMS;
        move.player_id = p;
        move.gems_taken.blue = 1;
        move.gems_taken.green = 1;
        move.gems_taken.red = 1;
        
        // This makes total 12 gems. Must return 2.
        move.gems_returned.white = 2;

        ValidationResult v = validateMove(state, move);
        INFO(v.error_message);
        REQUIRE(v.valid);

        applyMove(state, move);
        CHECK(state.players[p].tokens.total() == 10);
        CHECK(state.players[p].tokens.white == 7);
        CHECK(state.players[p].tokens.blue == 1);
    }

    SECTION("10-Token Limit: Invalid if no return") {
        // Player has 8 gems
        state.players[p].tokens.white = 8;
        Move move;
        move.type = TAKE_GEMS;
        move.player_id = p;
        move.gems_taken.blue = 1;
        move.gems_taken.green = 1;
        move.gems_taken.red = 1;
        
        // Total 11 is not okay without gems_returned
        ValidationResult v = validateMove(state, move);
        CHECK_FALSE(v.valid);
        INFO("Actual message: " << v.error_message);
        CHECK(v.error_message.find("10 gems") != string::npos);
        
        // Returning to hit 10 is okay
        move.gems_returned.white = 1;
        CHECK(validateMove(state, move).valid);
    }

    SECTION("Joker Logic: Cannot TAKE jokers") {
        Move move;
        move.type = TAKE_GEMS;
        move.player_id = p;
        move.gems_taken.joker = 1;

        ValidationResult v = validateMove(state, move);
        CHECK_FALSE(v.valid);
    }

    SECTION("Must take 3 if available") {
        // All 5 colors are available (bank initialized to 4 each)
        Move move;
        move.type = TAKE_GEMS;
        move.player_id = p;
        move.gems_taken.white = 1;
        move.gems_taken.blue = 1;
        // Only taking 2 when 5 are available
        
        ValidationResult v = validateMove(state, move);
        CHECK_FALSE(v.valid);
        INFO("Actual message: " << v.error_message);
        CHECK(v.error_message.find("Must take 3 gems") != string::npos);
        
        // Correcting to 3 gems
        move.gems_taken.green = 1;
        CHECK(validateMove(state, move).valid);
    }

    SECTION("Must take 2 if only 2 available") {
        state.bank.white = 4;
        state.bank.blue = 4;
        state.bank.green = 0;
        state.bank.red = 0;
        state.bank.black = 0;

        Move move;
        move.type = TAKE_GEMS;
        move.player_id = p;
        move.gems_taken.white = 1;
        // Should take 2 as only 2 are available
        
        ValidationResult v = validateMove(state, move);
        CHECK_FALSE(v.valid);
        INFO("Actual message: " << v.error_message);
        CHECK(v.error_message.find("Must take 2 gems") != string::npos);

        move.gems_taken.blue = 1;
        CHECK(validateMove(state, move).valid);
    }

    SECTION("Must take 1 if only 1 available") {
        state.bank.white = 4;
        state.bank.blue = 0;
        state.bank.green = 0;
        state.bank.red = 0;
        state.bank.black = 0;

        Move move;
        move.type = TAKE_GEMS;
        move.player_id = p;
        move.gems_taken.white = 1;
        
        CHECK(validateMove(state, move).valid);
    }
}

TEST_CASE("B. Purchasing (BUY moves)", "[buying]") {
    GameState state;
    {
        SilenceCerr sc;
        state.replay_mode = false;
        initializeGame(state, 123, "data/cards.json", "data/nobles.json");
    }
    int p = state.current_player;
    Card card = state.faceup_level1[0];

    SECTION("Exact Payment") {
        state.players[p].tokens = card.cost;
        Move move;
        move.type = BUY_CARD;
        move.player_id = p;
        move.card_id = card.id;
        move.auto_payment = true;

        ValidationResult v = validateMove(state, move);
        INFO(v.error_message);
        REQUIRE(v.valid);

        applyMove(state, move);
        CHECK(state.players[p].cards.size() == 1);
        CHECK(state.players[p].tokens.total() == 0);
    }

    SECTION("Bonus Discounts") {
        // Noble-cost style card: 3 white, 3 blue, 3 green
        card.cost = {0, 3, 3, 3, 0, 0}; // black, blue, white, green, red, joker
        state.faceup_level1[0] = card; // CRITICAL: Update board state
        state.players[p].bonuses = {0, 1, 1, 1, 0, 0}; // 1 discount for each
        state.players[p].tokens = {0, 2, 2, 2, 0, 0}; // Exactly enough with discounts
        
        Move move;
        move.type = BUY_CARD;
        move.player_id = p;
        move.card_id = card.id;
        move.auto_payment = true;

        ValidationResult v = validateMove(state, move);
        INFO(v.error_message);
        REQUIRE(v.valid);

        applyMove(state, move);
        CHECK(state.players[p].tokens.total() == 0);
    }

    SECTION("Joker Substitution") {
        card.cost = {1, 0, 0, 0, 0, 0}; // 1 black
        state.faceup_level1[0] = card; // CRITICAL: Update board state
        state.players[p].tokens.joker = 1;
        state.players[p].tokens.black = 0;
        
        Move move;
        move.type = BUY_CARD;
        move.player_id = p;
        move.card_id = card.id;
        move.auto_payment = true;

        ValidationResult v = validateMove(state, move);
        INFO(v.error_message);
        REQUIRE(v.valid);

        applyMove(state, move);
        CHECK(state.players[p].tokens.joker == 0);
    }

    SECTION("Overpayment Check") {
        card.cost = {1, 0, 0, 0, 0, 0}; // 1 black
        state.players[p].tokens.black = 2;
        
        Move move;
        move.type = BUY_CARD;
        move.player_id = p;
        move.card_id = card.id;
        move.auto_payment = false;
        move.payment.black = 2; // Trying to pay 2 for a cost of 1

        ValidationResult v = validateMove(state, move);
        CHECK_FALSE(v.valid);
        INFO("Actual message: " << v.error_message);
        CHECK(v.error_message.find("Overpaying") != string::npos);
    }

    SECTION("Reserved vs. Board") {
        // Move card from board to reserved
        Card reserved_card = state.faceup_level1[0];
        state.players[p].reserved.push_back(reserved_card);
        // Remove from faceup (normally applyMove(RESERVE) does this)
        state.faceup_level1.erase(state.faceup_level1.begin());
        
        state.players[p].tokens = reserved_card.cost;
        
        Move move;
        move.type = BUY_CARD;
        move.player_id = p;
        move.card_id = reserved_card.id;
        move.auto_payment = true;

        REQUIRE(validateMove(state, move).valid);
        applyMove(state, move);
        CHECK(state.players[p].cards.size() == 1);
        CHECK(state.players[p].reserved.size() == 0);
    }

    SECTION("Insufficient Tokens") {
        state.players[p].tokens = {0, 0, 0, 0, 0, 0};
        card.cost = {1, 1, 1, 1, 1, 0};
        state.faceup_level1[0] = card;

        Move move;
        move.type = BUY_CARD;
        move.player_id = p;
        move.card_id = card.id;
        move.auto_payment = true;

        ValidationResult v = validateMove(state, move);
        CHECK_FALSE(v.valid);
        // "Not enough jokers to cover cost" or "not enough tokens"
    }

    SECTION("Invalid Card ID") {
        Move move;
        move.type = BUY_CARD;
        move.player_id = p;
        move.card_id = 999;
        move.auto_payment = true;

        ValidationResult v = validateMove(state, move);
        CHECK_FALSE(v.valid);
        CHECK(v.error_message.find("not found") != std::string::npos);
    }

    SECTION("Opponent's Reserved Card") {
        Card res = state.faceup_level1[0];
        int opponent = (p + 1) % 2;
        state.players[opponent].reserved.push_back(res);
        state.faceup_level1.erase(state.faceup_level1.begin());

        state.players[p].tokens = res.cost;
        Move move;
        move.type = BUY_CARD;
        move.player_id = p;
        move.card_id = res.id;
        move.auto_payment = true;

        ValidationResult v = validateMove(state, move);
        CHECK_FALSE(v.valid);
    }

    SECTION("Card Not on Board or Reserved") {
        if (!state.deck_level1.empty()) {
            Card secret_card = state.deck_level1.back();
            state.deck_level1.pop_back();

            state.players[p].tokens = secret_card.cost;
            Move move;
            move.type = BUY_CARD;
            move.player_id = p;
            move.card_id = secret_card.id;
            move.auto_payment = true;

            ValidationResult v = validateMove(state, move);
            CHECK_FALSE(v.valid);
        }
    }
}

TEST_CASE("C. Card Reserving (RESERVE moves)", "[reserve]") {
    GameState state;
    {
        SilenceCerr sc;
        state.replay_mode = false;
        initializeGame(state, 123, "data/cards.json", "data/nobles.json");
    }
    int p = state.current_player;

    SECTION("Limit Check (Max 3)") {
        state.players[p].reserved.push_back(state.faceup_level1[0]);
        state.players[p].reserved.push_back(state.faceup_level1[1]);
        state.players[p].reserved.push_back(state.faceup_level2[0]);
        
        Move move;
        move.type = RESERVE_CARD;
        move.player_id = p;
        move.card_id = state.faceup_level3[0].id;

        ValidationResult v = validateMove(state, move);
        CHECK_FALSE(v.valid);
        INFO("Actual message: " << v.error_message);
        CHECK(v.error_message.find("3 reserved cards") != std::string::npos);
    }

    SECTION("Joker Reward") {
        state.bank.joker = 1;
        Move move;
        move.type = RESERVE_CARD;
        move.player_id = p;
        move.card_id = state.faceup_level1[0].id;

        applyMove(state, move);
        CHECK(state.players[p].tokens.joker == 1);
        CHECK(state.bank.joker == 0);
    }

    SECTION("Return newly acquired Joker") {
        // Player has 10 gems, no jokers
        state.players[p].tokens = {2, 2, 2, 2, 2, 0};
        state.bank.joker = 1;
        
        Move move;
        move.type = RESERVE_CARD;
        move.player_id = p;
        move.card_id = state.faceup_level1[0].id;
        // Return the joker we are about to get to stay at 10
        move.gems_returned.joker = 1;

        ValidationResult v = validateMove(state, move);
        INFO("Error message: " << v.error_message);
        REQUIRE(v.valid);
        
        applyMove(state, move);
        CHECK(state.players[p].tokens.joker == 0);
        CHECK(state.players[p].tokens.total() == 10);
    }

    SECTION("Return newly acquired gems (TAKE)") {
        // Player has 8 gems, no red gems
        state.players[p].tokens = {2, 2, 2, 2, 0, 0}; // black, blue, white, green, red, joker
        state.bank.white = 4;
        state.bank.blue = 4;
        state.bank.black = 4;
        
        Move move;
        move.type = TAKE_GEMS;
        move.player_id = p;
        move.gems_taken.white = 1;
        move.gems_taken.green = 1;
        move.gems_taken.red = 1;
        // Total gems after take = 8 + 3 = 11. Must return 1.
        // Returning the red gem we JUST took.
        move.gems_returned.red = 1;

        ValidationResult v = validateMove(state, move);
        INFO("Error message: " << v.error_message);
        REQUIRE(v.valid);
        
        applyMove(state, move);
        CHECK(state.players[p].tokens.red == 0);
        CHECK(state.players[p].tokens.total() == 10);
    }

    SECTION("Joker Reward: Empty Bank") {
        state.bank.joker = 0;
        Move move;
        move.type = RESERVE_CARD;
        move.player_id = p;
        move.card_id = state.faceup_level1[0].id;

        ValidationResult v = validateMove(state, move);
        CHECK(v.valid); // still valid to reserve
        applyMove(state, move);
        CHECK(state.players[p].tokens.joker == 0);
    }

    SECTION("Blind Reserve from Deck") {
        int initial_deck_size = state.deck_level1.size();
        Move move;
        move.type = RESERVE_CARD;
        move.player_id = p;
        move.card_id = 91; // Level 1 blind reserve

        REQUIRE(validateMove(state, move).valid);
        applyMove(state, move);
        
        CHECK(state.players[p].reserved.size() == 1);
        CHECK(state.deck_level1.size() == initial_deck_size - 1);
        // Faceup should not change
        CHECK(state.faceup_level1.size() == 4);
    }

    SECTION("Board Replacement after Reserve") {
        int target_id = state.faceup_level1[0].id;
        int initial_deck_size = state.deck_level1.size();
        
        Move move;
        move.type = RESERVE_CARD;
        move.player_id = p;
        move.card_id = target_id;

        applyMove(state, move);
        CHECK(state.faceup_level1.size() == 4); // replaced from deck
        CHECK(state.deck_level1.size() == initial_deck_size - 1);
        for(auto& c : state.faceup_level1) CHECK(c.id != target_id);
    }
}

TEST_CASE("D. Nobles and Win Conditions", "[nobles][win]") {
    GameState state;
    {
        SilenceCerr sc;
        state.replay_mode = false;
        initializeGame(state, 123, "data/cards.json", "data/nobles.json");
    }
    int p = state.current_player;

    SECTION("Noble Visit") {
        // Pick a noble and give requirements
        Noble n = state.available_nobles[0];
        state.players[p].bonuses = n.requirements;
        
        // Buying any card triggers noble check
        Card c = state.faceup_level1[0];
        state.players[p].tokens = c.cost;
        
        Move move;
        move.type = BUY_CARD;
        move.player_id = p;
        move.card_id = c.id;
        move.auto_payment = true;

        ValidationResult v = validateMove(state, move);
        INFO(v.error_message);
        REQUIRE(v.valid);

        applyMove(state, move);
        CHECK(state.players[p].nobles.size() == 1);
        CHECK(state.players[p].nobles[0].id == n.id);
        CHECK(state.players[p].points >= 3);
    }

    SECTION("Multiple Nobles - Selection") {
        // This is complex - usually requires user input if not auto-picked.
        // Assuming the logic requires noble_id if multiple qualify.
        if (state.available_nobles.size() >= 2) {
            Noble n1 = state.available_nobles[0];
            Noble n2 = state.available_nobles[1];
            (void)n1; // Suppress unused warning for n1 if only n2 is used
            
            // Give bonuses for both
            state.players[p].bonuses.white = 10;
            state.players[p].bonuses.blue = 10;
            state.players[p].bonuses.green = 10;
            state.players[p].bonuses.red = 10;
            state.players[p].bonuses.black = 10;
            
            Card c = state.faceup_level1[0];
            state.players[p].tokens = c.cost;
            
            Move move;
            move.type = BUY_CARD;
            move.player_id = p;
            move.card_id = c.id;
            move.auto_payment = true;
            move.noble_id = n2.id; // Explicitly pick second noble

            applyMove(state, move);
            CHECK(state.players[p].nobles.size() == 1);
            CHECK(state.players[p].nobles[0].id == n2.id);
        }
    }

    SECTION("Win Threshold (15 points)") {
        state.players[p].points = 14;
        Card c = state.faceup_level1[0];
        c.points = 1;
        state.faceup_level1[0] = c;
        state.players[p].tokens = c.cost;
        
        Move move;
        move.type = BUY_CARD;
        move.player_id = p;
        move.card_id = c.id;
        move.auto_payment = true;

        applyMove(state, move);
        CHECK(state.players[p].points == 15);
        
        if (p == 0) {
            // If player 0 reaches 15, player 1 gets one more turn
            CHECK_FALSE(isGameOver(state));
            state.current_player = 0; // Simulate player 1 finishing turn
            CHECK(isGameOver(state));
        } else {
            // If player 1 reaches 15, game ends immediately
            CHECK(isGameOver(state));
        }
    }

    SECTION("Tie-breaking: Fewest Cards") {
        // Setup tie: both have 15 points
        state.players[0].points = 15;
        state.players[0].cards.clear();
        state.players[0].cards.resize(10); // 10 cards
        
        state.players[1].points = 15;
        state.players[1].cards.clear();
        state.players[1].cards.resize(8); // 8 cards
        
        CHECK(determineWinner(state) == 1); // Player 1 wins
    }
}

TEST_CASE("Edge Cases", "[edge]") {
    GameState state;
    {
        SilenceCerr sc;
        state.replay_mode = false;
        initializeGame(state, 123, "data/cards.json", "data/nobles.json");
    }
    int p = state.current_player;

    SECTION("Returning tokens you don't have") {
        state.players[p].tokens.white = 0;
        Move move;
        move.type = TAKE_GEMS;
        move.player_id = p;
        move.gems_returned.white = 1;
        
        ValidationResult v = validateMove(state, move);
        CHECK_FALSE(v.valid);
    }

    SECTION("Reserving when decks are empty") {
        state.deck_level1.clear();
        int initial_faceup = state.faceup_level1.size();
        
        Move move;
        move.type = RESERVE_CARD;
        move.player_id = p;
        move.card_id = state.faceup_level1[0].id;

        applyMove(state, move);
        CHECK(state.faceup_level1.size() == initial_faceup);
        CHECK(state.faceup_level1[0].id == 0); // Placeholder
        CHECK(state.players[p].reserved.size() == 1);
    }
    
    SECTION("Taking 2 gems when you only have room for 1") {
        state.players[p].tokens.white = 9;
        Move move;
        move.type = TAKE_GEMS;
        move.player_id = p;
        move.gems_taken.blue = 2; // Total would be 11
        
        // Invalid without returning at least 1
        CHECK_FALSE(validateMove(state, move).valid);
        
        move.gems_returned.white = 1;
        CHECK(validateMove(state, move).valid);
    }
}
