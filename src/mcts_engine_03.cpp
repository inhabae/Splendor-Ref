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

// Evaluation Weights
const double W_CARD = 1.0;
const double W_GEM = 0.2;
const double W_JOKER = 0.5;
const double W_POINT = 10.0;
const double W_NOBLE = 5.0;
const double W_NOBLE_PROGRESS = 0.4;
const double W_RESERVED_PROGRESS = 0.25;

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
        playerWhoMoved = 1 - state.current_player; // The player who just moved to get to this state
    }

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
    
    uniform_int_distribution<int> dist(0, node->untriedMoves.size() - 1);
    int idx = dist(rng);
    Move move = node->untriedMoves[idx];
    node->untriedMoves.erase(node->untriedMoves.begin() + idx);

    GameState nextState = node->state;
    applyMove(nextState, move);
    
    node->children.push_back(unique_ptr<MCTSNode>(new MCTSNode(nextState, node, move)));
    return node->children.back().get();
}

double simulate(GameState state, mt19937& rng) {
    int evaluation_player = 1 - state.current_player; // The player who just moved to get here
    
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

Move mctsSearch(const GameState& rootState, int iterations) {
    mt19937 rng(time(0));
    unique_ptr<MCTSNode> root(new MCTSNode(rootState));

    if (root->untriedMoves.size() == 1) return root->untriedMoves[0];

    for (int i = 0; i < iterations; i++) {
        MCTSNode* leaf = select(root.get());
        if (!leaf->isTerminal() && !leaf->untriedMoves.empty()) {
            leaf = expand(leaf, rng);
        }
        double result = simulate(leaf->state, rng);
        backpropagate(leaf, result);
    }

    MCTSNode* bestChild = nullptr;
    int maxVisits = -1;
    for (auto& child : root->children) {
        if (child->visits > maxVisits) {
            maxVisits = child->visits;
            bestChild = child.get();
        }
    }

    return bestChild ? bestChild->move : (root->untriedMoves.empty() ? Move() : root->untriedMoves[0]);
}

int main() {
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
            
            if (active == you) {
                GameState st = parseJson(line, all_c, all_n);
                Move bestMove = mctsSearch(st, ITERATIONS);
                string moveStr = moveToString(bestMove);
                if (moveStr.empty()) moveStr = "PASS";
                cout << moveStr << endl << flush;
                cerr << "MCTS Engine output: " << moveStr << endl;
            }
        } catch (const exception& e) {
            cerr << "MCTS Engine error: " << e.what() << endl;
        } catch (...) {
            cerr << "MCTS Engine error: unknown" << endl;
        }
    }
    return 0;
}
