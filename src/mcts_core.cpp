#include "mcts_core.h"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <limits>
#include <map>
#include <numeric>
#include <random>
#include <string>
#include <vector>

namespace {

struct Node {
    int parent = -1;
    Move move_from_parent;
    int player_to_move = 0;
    int visits = 0;
    double value_sum = 0.0;
    double prior = 1.0;
    std::vector<Move> untried;
    std::vector<int> children;
};

struct RootActionResult {
    Move move;
    int visits;
    double mean;
};

double clampUnit(double x) {
    if (x > 1.0) return 1.0;
    if (x < -1.0) return -1.0;
    return x;
}

double normalizeValue(double raw) {
    // Keep UCT value bounded to reduce numeric instability.
    return clampUnit(std::tanh(raw / 120.0));
}

int selectChildPUCT(const std::vector<Node>& tree, int node_idx, double c_puct, std::mt19937& rng, int root_player) {
    const Node& node = tree[node_idx];
    double best_score = -std::numeric_limits<double>::infinity();
    std::vector<int> best_children;

    const double parent_scale = std::sqrt(static_cast<double>(node.visits) + 1.0);

    for (size_t i = 0; i < node.children.size(); ++i) {
        int child_idx = node.children[i];
        const Node& child = tree[child_idx];

        const double q = (child.visits > 0) ? (child.value_sum / child.visits) : 0.0;
        const double q_for_player = (node.player_to_move == root_player) ? q : -q;
        const double u = c_puct * child.prior * parent_scale / (1.0 + child.visits);
        const double score = q_for_player + u;

        if (score > best_score + 1e-12) {
            best_score = score;
            best_children.clear();
            best_children.push_back(child_idx);
        } else if (std::fabs(score - best_score) <= 1e-12) {
            best_children.push_back(child_idx);
        }
    }

    if (best_children.empty()) return -1;
    if (best_children.size() == 1) return best_children[0];

    std::uniform_int_distribution<int> pick(0, static_cast<int>(best_children.size()) - 1);
    return best_children[pick(rng)];
}

std::vector<RootActionResult> runSingleWorldMCTS(const GameState& root_state,
                                                 int root_player,
                                                 int sims,
                                                 const MctsConfig& cfg,
                                                 const EvalWeights& weights,
                                                 std::mt19937& rng) {
    std::vector<Node> tree;
    tree.reserve(std::max(32, sims * 2));

    Node root;
    root.parent = -1;
    root.player_to_move = root_state.current_player;
    root.untried = findAllValidMoves(root_state);
    tree.push_back(root);

    for (int sim = 0; sim < sims; ++sim) {
        GameState state = root_state;
        int node_idx = 0;
        int depth = 0;
        std::vector<int> path;
        path.push_back(node_idx);

        while (true) {
            if (isGameOver(state) || depth >= cfg.max_depth) {
                break;
            }

            Node& node = tree[node_idx];

            if (!node.untried.empty()) {
                std::uniform_int_distribution<int> pick(0, static_cast<int>(node.untried.size()) - 1);
                int move_idx = pick(rng);

                Move m = node.untried[move_idx];
                node.untried[move_idx] = node.untried.back();
                node.untried.pop_back();

                ValidationResult ar = applyMove(state, m);
                if (!ar.valid) {
                    break;
                }

                Node child;
                child.parent = node_idx;
                child.move_from_parent = m;
                child.player_to_move = state.current_player;
                child.untried = findAllValidMoves(state);

                const int new_idx = static_cast<int>(tree.size());
                tree.push_back(child);
                tree[node_idx].children.push_back(new_idx);

                node_idx = new_idx;
                path.push_back(node_idx);
                depth++;
                break;
            }

            if (node.children.empty()) {
                break;
            }

            int child_idx = selectChildPUCT(tree, node_idx, cfg.c_puct, rng, root_player);
            if (child_idx < 0) break;

            ValidationResult ar = applyMove(state, tree[child_idx].move_from_parent);
            if (!ar.valid) {
                break;
            }

            node_idx = child_idx;
            path.push_back(node_idx);
            depth++;
        }

        double value = evaluate_state(state, root_player, weights);
        value = normalizeValue(value);

        for (size_t i = 0; i < path.size(); ++i) {
            Node& n = tree[path[i]];
            n.visits += 1;
            n.value_sum += value;
        }
    }

    std::vector<RootActionResult> out;
    for (size_t i = 0; i < tree[0].children.size(); ++i) {
        const Node& c = tree[tree[0].children[i]];
        RootActionResult r;
        r.move = c.move_from_parent;
        r.visits = c.visits;
        r.mean = (c.visits > 0) ? (c.value_sum / c.visits) : 0.0;
        out.push_back(r);
    }

    if (out.empty()) {
        std::vector<Move> fallback = findAllValidMoves(root_state);
        if (!fallback.empty()) {
            RootActionResult r;
            r.move = fallback[0];
            r.visits = 1;
            r.mean = 0.0;
            out.push_back(r);
        }
    }

    return out;
}

struct Aggregate {
    Move move;
    int total_visits = 0;
    double weighted_sum = 0.0;
    int weighted_n = 0;
    std::vector<double> deterministic_means;
};

double stdev(const std::vector<double>& vals) {
    if (vals.size() < 2) return 0.0;
    double mean = std::accumulate(vals.begin(), vals.end(), 0.0) / vals.size();
    double var = 0.0;
    for (size_t i = 0; i < vals.size(); ++i) {
        double d = vals[i] - mean;
        var += d * d;
    }
    var /= vals.size();
    return std::sqrt(var);
}

} // namespace

Move select_mcts_move(const GameState& state,
                      int root_player,
                      const MctsConfig& cfg,
                      const EvalWeights& weights,
                      BeliefState& belief_state) {
    if (cfg.simulations <= 0) {
        std::vector<Move> legal = findAllValidMoves(state);
        return legal.empty() ? Move() : legal[0];
    }

    std::mt19937 rng(static_cast<unsigned int>(cfg.seed == 0 ? time(nullptr) : cfg.seed));

    const int det_count = std::max(1, cfg.determinizations_per_batch);
    const int sims_per_det = std::max(1, cfg.simulations / det_count);

    std::map<std::string, Aggregate> by_move;

    for (int d = 0; d < det_count; ++d) {
        GameState world = belief_state.sampleDeterminization(state, root_player);
        std::vector<RootActionResult> results = runSingleWorldMCTS(world, root_player, sims_per_det, cfg, weights, rng);

        for (size_t i = 0; i < results.size(); ++i) {
            const RootActionResult& rr = results[i];
            const std::string key = moveToString(rr.move);
            Aggregate& ag = by_move[key];
            ag.move = rr.move;
            ag.total_visits += rr.visits;
            ag.weighted_sum += rr.mean * std::max(1, rr.visits);
            ag.weighted_n += std::max(1, rr.visits);
            ag.deterministic_means.push_back(rr.mean);
        }
    }

    if (by_move.empty()) {
        std::vector<Move> legal = findAllValidMoves(state);
        return legal.empty() ? Move() : legal[0];
    }

    bool found = false;
    Move best_move;
    int best_visits = -1;
    double best_score = -std::numeric_limits<double>::infinity();
    std::string best_key;

    for (std::map<std::string, Aggregate>::const_iterator it = by_move.begin(); it != by_move.end(); ++it) {
        const std::string& key = it->first;
        const Aggregate& ag = it->second;

        const double mean = (ag.weighted_n > 0) ? (ag.weighted_sum / ag.weighted_n) : 0.0;
        const double risk = stdev(ag.deterministic_means);
        const double conservative_score = mean - cfg.risk_lambda * risk;

        if (!found ||
            ag.total_visits > best_visits ||
            (ag.total_visits == best_visits && conservative_score > best_score + 1e-12) ||
            (ag.total_visits == best_visits && std::fabs(conservative_score - best_score) <= 1e-12 && key < best_key)) {
            found = true;
            best_move = ag.move;
            best_visits = ag.total_visits;
            best_score = conservative_score;
            best_key = key;
        }
    }

    if (found) return best_move;

    std::vector<Move> legal = findAllValidMoves(state);
    return legal.empty() ? Move() : legal[0];
}
