#include "belief_state.h"
#include "game_logic.h"
#include "linear_eval.h"
#include "mcts_core.h"

#include <exception>
#include <iostream>
#include <random>
#include <string>
#include <ctime>
#include <vector>

using std::cerr;
using std::cin;
using std::cout;
using std::endl;
using std::getline;
using std::size_t;
using std::string;
using std::vector;

namespace {

bool extractIntField(const string& s, const string& key, int& out) {
    size_t p = s.find(key);
    if (p == string::npos) return false;
    p += key.size();
    size_t e = s.find_first_of(",}", p);
    if (e == string::npos) return false;
    try {
        out = std::stoi(s.substr(p, e - p));
        return true;
    } catch (...) {
        return false;
    }
}

void applyWeightByIndex(EvalWeights& w, int idx, double v) {
    if (idx == 0) w.W_POINT_SELF = v;
    else if (idx == 1) w.W_POINT_OPP = v;
    else if (idx == 2) w.W_BONUS_SELF = v;
    else if (idx == 3) w.W_BONUS_OPP = v;
    else if (idx == 4) w.W_RESERVED_SELF = v;
    else if (idx == 5) w.W_RESERVED_OPP = v;
    else if (idx == 6) w.W_NOBLE_PROGRESS_SELF = v;
    else if (idx == 7) w.W_NOBLE_PROGRESS_OPP = v;
    else if (idx == 8) w.W_AFFORDABLE_SELF = v;
    else if (idx == 9) w.W_AFFORDABLE_OPP = v;
    else if (idx == 10) w.W_WIN_BONUS = v;
    else if (idx == 11) w.W_LOSS_PENALTY = v;
    else if (idx == 12) w.W_TURN_PENALTY = v;
    else if (idx == 13) w.W_EFFICIENCY = v;
    else if (idx == 14) w.W_DIR_FOCUS = v;
    else if (idx == 15) w.W_DIR_PROGRESS = v;
    else if (idx == 16) w.W_DIR_SPREAD = v;
    else if (idx == 17) w.W_DIR_RESERVE_MATCH = v;
    else if (idx == 18) w.W_DIR_SUPPORT_MATCH = v;
    else if (idx == 19) w.W_DIR_SLOT_PENALTY = v;
}

} // namespace

#ifndef ENGINE_TEST
int main(int argc, char* argv[]) {
    MctsConfig cfg;
    EvalWeights weights;

    vector<double> positional_weights;

    for (int i = 1; i < argc; ++i) {
        string arg = argv[i];

        try {
            if (arg == "--sims" && i + 1 < argc) {
                cfg.simulations = std::max(1, std::stoi(argv[++i]));
                continue;
            }
            if (arg == "--seed" && i + 1 < argc) {
                cfg.seed = static_cast<uint64_t>(std::stoull(argv[++i]));
                continue;
            }
            if (arg == "--max-depth" && i + 1 < argc) {
                cfg.max_depth = std::max(1, std::stoi(argv[++i]));
                continue;
            }
            if (arg == "--risk-lambda" && i + 1 < argc) {
                cfg.risk_lambda = std::stod(argv[++i]);
                continue;
            }
            if (arg == "--det" && i + 1 < argc) {
                cfg.determinizations_per_batch = std::max(1, std::stoi(argv[++i]));
                continue;
            }
            if (arg == "--c-puct" && i + 1 < argc) {
                cfg.c_puct = std::stod(argv[++i]);
                continue;
            }

            // Unflagged numerics are interpreted as ordered weight overrides.
            positional_weights.push_back(std::stod(arg));
        } catch (...) {
            // Ignore non-numeric extras for compatibility with old runner args.
        }
    }

    for (size_t i = 0; i < positional_weights.size() && i < 20; ++i) {
        applyWeightByIndex(weights, static_cast<int>(i), positional_weights[i]);
    }

    vector<Card> all_cards = loadCards("data/cards.json");
    vector<Noble> all_nobles = loadNobles("data/nobles.json");
    if (all_cards.empty() || all_nobles.empty()) {
        cerr << "mcts_engine_01: failed to load data files" << endl;
        return 1;
    }

    BeliefState belief(all_cards, static_cast<unsigned int>(cfg.seed));

    uint64_t runtime_seed = cfg.seed;
    if (runtime_seed == 0) {
        runtime_seed = static_cast<uint64_t>(time(nullptr));
    }

    string line;
    while (getline(cin, line)) {
        if (line.empty()) continue;

        int you = 0;
        int active = 0;
        if (!extractIntField(line, "\"you\":", you)) continue;
        if (!extractIntField(line, "\"active_player_id\":", active)) continue;

        if (you <= 0 || active <= 0) continue;
        if (you != active) continue;

        try {
            GameState st = parseJson(line, all_cards, all_nobles);

            MctsConfig turn_cfg = cfg;
            turn_cfg.seed = runtime_seed++;

            Move chosen = select_mcts_move(st, you - 1, turn_cfg, weights, belief);
            cout << moveToString(chosen) << endl << std::flush;
        } catch (const std::exception& e) {
            cerr << "mcts_engine_01 error: " << e.what() << endl;
            cout << "PASS" << endl << std::flush;
        } catch (...) {
            cerr << "mcts_engine_01 error: unknown" << endl;
            cout << "PASS" << endl << std::flush;
        }
    }

    return 0;
}
#endif
