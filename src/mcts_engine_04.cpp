#include "game_logic.h"
#include <vector>
#include <algorithm>
#include <string>
#include <iostream>
#include <random>
#include <ctime>
#include <chrono>
#include <cmath>
#include <memory>

using namespace std;
using namespace std::chrono;

const double UCT_CONSTANT = 1.414;
const int ITERATIONS = 2000;

// Evaluation Weights (defaults)
double W_CARD = 0.4;
double W_GEM = 0.25;
double W_JOKER = 0.3;
double W_POINT = 20.13;
double W_NOBLE = 20.7;
double W_NOBLE_PROGRESS = 0.1;
double W_RESERVED_PROGRESS = 0.1;
double W_RESERVED_EFFICIENCY = 1.06;
double W_UNRESERVED_SLOT = 0.5;
double W_BOUGHT_EFFICIENCY = 1.0;

double DEFAULT_CARD_EFFICIENCY = 0.25; // Baseline for bought card efficiency

bool isSamePlayer(const Player& p1, const Player& p2) {
    if (p1.points != p2.points) return false;
    if (p1.tokens != p2.tokens) return false;
    if (p1.bonuses != p2.bonuses) return false;
    if (p1.cards.size() != p2.cards.size()) return false;
    if (p1.reserved.size() != p2.reserved.size()) return false;
    if (p1.nobles.size() != p2.nobles.size()) return false;
    // Assume cards/reserved/nobles are in same order if same moves were made
    for (size_t i = 0; i < p1.cards.size(); i++) if (p1.cards[i].id != p2.cards[i].id) return false;
    for (size_t i = 0; i < p1.reserved.size(); i++) if (p1.reserved[i].id != p2.reserved[i].id) return false;
    for (size_t i = 0; i < p1.nobles.size(); i++) if (p1.nobles[i].id != p2.nobles[i].id) return false;
    return true;
}

bool isSameState(const GameState& s1, const GameState& s2) {
    if (s1.current_player != s2.current_player) return false;
    if (s1.bank != s2.bank) return false;
    if (!isSamePlayer(s1.players[0], s2.players[0])) return false;
    if (!isSamePlayer(s1.players[1], s2.players[1])) return false;
    
    if (s1.faceup_level1.size() != s2.faceup_level1.size()) return false;
    for (size_t i = 0; i < s1.faceup_level1.size(); i++) if (s1.faceup_level1[i].id != s2.faceup_level1[i].id) return false;
    if (s1.faceup_level2.size() != s2.faceup_level2.size()) return false;
    for (size_t i = 0; i < s1.faceup_level2.size(); i++) if (s1.faceup_level2[i].id != s2.faceup_level2[i].id) return false;
    if (s1.faceup_level3.size() != s2.faceup_level3.size()) return false;
    for (size_t i = 0; i < s1.faceup_level3.size(); i++) if (s1.faceup_level3[i].id != s2.faceup_level3[i].id) return false;
    if (s1.available_nobles.size() != s2.available_nobles.size()) return false;
    for (size_t i = 0; i < s1.available_nobles.size(); i++) if (s1.available_nobles[i].id != s2.available_nobles[i].id) return false;
    return true;
}

struct MCTSNode {
    GameState state;
    Move move;
    MCTSNode* parent;
    vector<unique_ptr<MCTSNode>> children;
    double wins = 0;
    int visits = 0;
    vector<Move> untriedMoves;
    int playerWhoMoved;

    MCTSNode(const GameState& s, MCTSNode* p = nullptr, Move m = Move()) 
        : state(s), move(m), parent(p) {
        untriedMoves = findAllValidMoves(state);
        playerWhoMoved = 1 - state.current_player;
        
        // Debug: Log detailed state when generating moves
        int p_idx = state.current_player;
        cerr << "[MCTSNode] Created for Player " << (p_idx + 1) << ": "
             << state.players[p_idx].tokens.total() << " gems, "
             << state.players[p_idx].reserved.size() << " reserved cards, "
             << state.players[p_idx].cards.size() << " bought cards, "
             << untriedMoves.size() << " valid moves generated" << endl;
        
        // Log first few moves to see what was generated
        if (untriedMoves.size() <= 5) {
            for (size_t i = 0; i < untriedMoves.size(); i++) {
                cerr << "  Move " << i << ": type=" << untriedMoves[i].type << " card_id=" << untriedMoves[i].card_id << endl;
            }
        }
    }
    
    // Prevent copying (tree structure should not be copied)
    MCTSNode(const MCTSNode&) = delete;
    MCTSNode& operator=(const MCTSNode&) = delete;

    double uctScore(int totalVisits) const {
        if (visits == 0) return 1e9;
        return (wins / visits) + UCT_CONSTANT * sqrt(log(totalVisits) / visits);
    }

    bool isTerminal() const {
        return isGameOver(state);
    }

    bool isFullyExpanded() const {
        return untriedMoves.empty();
    }
};

MCTSNode* select(MCTSNode* node) {
    while (!node->isTerminal()) {
        if (!node->isFullyExpanded()) {
            return node;
        } else if (node->children.empty()) {
            return node; // Should not happen if not terminal
        } else {
            MCTSNode* bestChild = nullptr;
            double bestScore = -1e18;
            for (auto& child : node->children) {
                double score = child->uctScore(node->visits);
                if (score > bestScore) {
                    bestScore = score;
                    bestChild = child.get();
                }
            }
            node = bestChild;
        }
    }
    return node;
}

MCTSNode* expand(MCTSNode* node, mt19937& rng) {
    if (node->isTerminal()) return node;
    if (node->untriedMoves.empty()) return node;
    
    uniform_int_distribution<int> dist(0, node->untriedMoves.size() - 1);
    int idx = dist(rng);
    Move move = node->untriedMoves[idx];
    node->untriedMoves.erase(node->untriedMoves.begin() + idx);

    GameState nextState = node->state;
    ValidationResult result = applyMove(nextState, move);
    
    // Only add to tree if move was valid
    if (!result.valid) {
        cerr << "Warning: Invalid move in expand: " << result.error_message << endl;
        return node;
    }
    
    node->children.push_back(unique_ptr<MCTSNode>(new MCTSNode(nextState, node, move)));
    return node->children.back().get();
}

double simulate(GameState state) {
    int evaluation_player = 1 - state.current_player;
    
    // If the state is terminal, return the actual result
    if (isGameOver(state)) {
        int winner = determineWinner(state);
        if (winner == -1) return 0.5; // Tie
        return (winner == evaluation_player) ? 1.0 : 0.0;
    }

    // Otherwise, use a linear sum of weights for heuristic evaluation
    auto getScore = [&](int p_idx) {
        const Player& p = state.players[p_idx];
        const Player& opp = state.players[1 - p_idx];
        double s = 0;
        s += p.cards.size() * W_CARD;
        s += p.tokens.total() * W_GEM;
        s += p.tokens.joker * W_JOKER;
        s += p.points * W_POINT;
        s += p.nobles.size() * W_NOBLE;

        // Noble progress
        for (const auto& noble : state.available_nobles) {
            int p_prog = 0;
            p_prog += min(p.bonuses.black, noble.requirements.black);
            p_prog += min(p.bonuses.blue, noble.requirements.blue);
            p_prog += min(p.bonuses.white, noble.requirements.white);
            p_prog += min(p.bonuses.green, noble.requirements.green);
            p_prog += min(p.bonuses.red, noble.requirements.red);

            int opp_prog = 0;
            opp_prog += min(opp.bonuses.black, noble.requirements.black);
            opp_prog += min(opp.bonuses.blue, noble.requirements.blue);
            opp_prog += min(opp.bonuses.white, noble.requirements.white);
            opp_prog += min(opp.bonuses.green, noble.requirements.green);
            opp_prog += min(opp.bonuses.red, noble.requirements.red);

            s += max(0, p_prog - opp_prog) * W_NOBLE_PROGRESS;
        }

        // Reserved progress
        for (const auto& card : p.reserved) {
            int prog = 0;
            prog += min(p.bonuses.black + p.tokens.black, card.cost.black);
            prog += min(p.bonuses.blue + p.tokens.blue, card.cost.blue);
            prog += min(p.bonuses.white + p.tokens.white, card.cost.white);
            prog += min(p.bonuses.green + p.tokens.green, card.cost.green);
            prog += min(p.bonuses.red + p.tokens.red, card.cost.red);
            s += prog * W_RESERVED_PROGRESS;

            // Reserved efficiency
            int total_cost = card.cost.total();
            if (total_cost > 0) {
                double efficiency = (card.points > 0) ? (double)card.points / total_cost : 1.0 / total_cost;
                if (efficiency < DEFAULT_CARD_EFFICIENCY) {
                    s -= efficiency * W_RESERVED_EFFICIENCY;
                } else {
                    s += efficiency * W_RESERVED_EFFICIENCY;
                }
            }
        }  

        // Unreserved slots reward
        s += (3 - (int)p.reserved.size()) * W_UNRESERVED_SLOT;

        // Bought cards efficiency
        for (const auto& card : p.cards) {
            int total_cost = card.cost.total();
            if (total_cost > 0) {
                double efficiency = (card.points > 0) ? (double)card.points / total_cost : 1.0 / total_cost;
                if (efficiency < DEFAULT_CARD_EFFICIENCY) {
                    s -= efficiency * W_BOUGHT_EFFICIENCY;
                } else {
                    s += efficiency * W_BOUGHT_EFFICIENCY;
                }
            }
        }

        return s;
    };

    double my_score = getScore(evaluation_player);
    double opp_score = getScore(1 - evaluation_player);
    
    // Normalize the difference using a sigmoid function to stay in [0, 1] for MCTS
    double diff = my_score - opp_score;
    return 1.0 / (1.0 + exp(-diff / 20.0));
}

void backpropagate(MCTSNode* node, double result) {
    while (node != nullptr) {
        node->visits++;
        node->wins += result;
        result = 1.0 - result; // Flip result for the other player
        node = node->parent;
    }
}

unique_ptr<MCTSNode> global_root;

void updateRoot(const GameState& st) {
    int p_idx = st.current_player;
    cerr << "\n[updateRoot] Called for Player " << (p_idx + 1) << ": "
         << st.players[p_idx].tokens.total() << " gems, "
         << st.players[p_idx].reserved.size() << " reserved, "
         << st.players[p_idx].cards.size() << " bought" << endl;
    
    if (!global_root) {
        cerr << "[updateRoot] Creating FIRST root node" << endl;
        global_root.reset(new MCTSNode(st));
        return;
    }
    
    if (isSameState(global_root->state, st)) {
        cerr << "[updateRoot] State UNCHANGED, keeping current root with "
             << global_root->untriedMoves.size() << " untried moves, "
             << global_root->children.size() << " children" << endl;
        return;
    }
    
    // Try to find st in children
    cerr << "[updateRoot] Searching " << global_root->children.size() << " children for match..." << endl;
    for (size_t i = 0; i < global_root->children.size(); i++) {
        if (isSameState(global_root->children[i]->state, st)) {
            cerr << "[updateRoot] FOUND MATCH at child " << i << " - REUSING TREE!" << endl;
            cerr << "[updateRoot] OLD untried moves: " << global_root->children[i]->untriedMoves.size() << endl;
            
            unique_ptr<MCTSNode> nextRoot = std::move(global_root->children[i]);
            global_root = std::move(nextRoot);
            global_root->parent = nullptr;
            
            // CRITICAL FIX: Regenerate untriedMoves for the reused node
            // The old untriedMoves may be stale from a previous MCTS simulation
            global_root->untriedMoves = findAllValidMoves(global_root->state);
            
            cerr << "[updateRoot] REGENERATED untried moves: " << global_root->untriedMoves.size() << endl;
            cerr << "[updateRoot] Reused node Player " << (st.current_player + 1) << " with "
                 << st.players[st.current_player].reserved.size() << " reserved cards" << endl;
            return;
        }
    }
    
    cerr << "[updateRoot] NO MATCH found, creating FRESH root" << endl;
    global_root.reset(new MCTSNode(st));
}

Move mctsSearch(int iterations) {
    if (!global_root) {
        cerr << "[mctsSearch] ERROR: null root" << endl;
        return Move();
    }
    
    cerr << "[mctsSearch] Root state: Player " << (global_root->state.current_player + 1) 
         << " with " << global_root->untriedMoves.size() << " untried moves, "
         << global_root->children.size() << " children" << endl;
    
    // Better RNG seeding using high-resolution clock
    auto now = high_resolution_clock::now();
    auto nanos = duration_cast<nanoseconds>(now.time_since_epoch()).count();
    mt19937 rng(static_cast<unsigned int>(nanos));

    if (global_root->untriedMoves.size() == 1 && global_root->children.empty()) {
        cerr << "[mctsSearch] Returning SINGLE untried move directly" << endl;
        return global_root->untriedMoves[0];
    }
    if (global_root->untriedMoves.empty() && global_root->children.empty()) {
        cerr << "[mctsSearch] WARNING: No valid moves available at all!" << endl;
        return Move();
    }

    for (int i = 0; i < iterations; i++) {
        MCTSNode* leaf = select(global_root.get());
        if (!leaf->isTerminal() && !leaf->untriedMoves.empty()) {
            leaf = expand(leaf, rng);
        }
        double result = simulate(leaf->state);
        backpropagate(leaf, result);
    }

    cerr << "[mctsSearch] After " << iterations << " iterations: root has " 
         << global_root->children.size() << " children" << endl;

    MCTSNode* bestChild = nullptr;
    int maxVisits = -1;
    for (auto& child : global_root->children) {
        if (child->visits > maxVisits) {
            maxVisits = child->visits;
            bestChild = child.get();
        }
    }

    if (bestChild) {
        cerr << "[mctsSearch] Returning BEST CHILD with " << maxVisits << " visits (type=" 
             << bestChild->move.type << ", card_id=" << bestChild->move.card_id << ")" << endl;
        return bestChild->move;
    } else if (!global_root->untriedMoves.empty()) {
        cerr << "[mctsSearch] WARNING: No children exist after MCTS!" << endl;
        cerr << "[mctsSearch] Validating " << global_root->untriedMoves.size() 
             << " untried moves..." << endl;
        
        // Try untried moves until we find a valid one
        while (!global_root->untriedMoves.empty()) {
            Move candidate = global_root->untriedMoves[0];
            global_root->untriedMoves.erase(global_root->untriedMoves.begin());
            
            cerr << "[mctsSearch] Testing untried move: type=" << candidate.type 
                 << ", card_id=" << candidate.card_id << endl;
            
            // Validate the move before returning it
            GameState test_state = global_root->state;
            ValidationResult result = applyMove(test_state, candidate);
            
            if (result.valid) {
                cerr << "[mctsSearch] Found VALID untried move!" << endl;
                return candidate;
            } else {
                cerr << "[mctsSearch] INVALID: " << result.error_message << endl;
            }
        }
        
        cerr << "[mctsSearch] ERROR: ALL untried moves were invalid!" << endl;
        return Move(); // Return PASS
    } else {
        cerr << "[mctsSearch] ERROR: No moves available at all!" << endl;
        return Move();
    }
}

int main(int argc, char* argv[]) {
    if (argc >= 11) {
        W_CARD = atof(argv[1]);
        W_GEM = atof(argv[2]);
        W_JOKER = atof(argv[3]);
        W_POINT = atof(argv[4]);
        W_NOBLE = atof(argv[5]);
        W_NOBLE_PROGRESS = atof(argv[6]);
        W_RESERVED_PROGRESS = atof(argv[7]);
        W_RESERVED_EFFICIENCY = atof(argv[8]);
        W_UNRESERVED_SLOT = atof(argv[9]);
        W_BOUGHT_EFFICIENCY = atof(argv[10]);
        cerr << "Weights loaded from command line" << endl;
    }
    cerr << "MCTS Engine started" << endl;
    vector<Card> all_c = loadCards("data/cards.json");
    vector<Noble> all_n = loadNobles("data/nobles.json");
    string line;
    while (getline(cin, line)) {
        if (line.empty()) continue;
        try {
            size_t you_p = line.find("\"you\":"); if (you_p == string::npos) continue;
            int you = stoi(line.substr(you_p + 6, line.find_first_of(",}", you_p + 6) - (you_p + 6)));
            size_t act_p = line.find("\"active_player_id\":"); if (act_p == string::npos) continue;
            int active = stoi(line.substr(act_p + 19, line.find_first_of(",}", act_p + 19) - (act_p + 19)));
            
            GameState st = parseJson(line, all_c, all_n);
            
            // Debug: Log parsed state details
            cerr << "\n=== TURN START: You=" << you << ", Active=" << active << " ===" << endl;
            for (int i = 0; i < 2; i++) {
                cerr << "Player " << (i+1) << ": " << st.players[i].tokens.total() << " gems, "
                     << st.players[i].reserved.size() << " reserved [";
                for (size_t j = 0; j < st.players[i].reserved.size(); j++) {
                    if (j > 0) cerr << ",";
                    cerr << st.players[i].reserved[j].id;
                }
                cerr << "], " << st.players[i].cards.size() << " bought" << endl;
            }
            
            updateRoot(st);

            if (active == you) {
                cerr << "\n[mctsSearch] Starting search..." << endl;
                Move bestMove = mctsSearch(ITERATIONS);
                string moveStr = moveToString(bestMove);
                if (moveStr.empty()) moveStr = "PASS";
                cout << moveStr << endl << flush;
                cerr << "[OUTPUT] " << moveStr << " (type=" << bestMove.type << ", card_id=" << bestMove.card_id << ")" << endl;
            }
        } catch (const exception& e) {
            cerr << "MCTS Engine error: " << e.what() << endl;
        } catch (...) {
            cerr << "MCTS Engine error: unknown" << endl;
        }
    }
    return 0;
}
