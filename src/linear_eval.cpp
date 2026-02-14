#include "linear_eval.h"

#include <algorithm>
#include <cmath>

namespace {
const double EFFICIENCY_THRESHOLD = 0.24;

int bonusTotal(const Tokens& t) {
    return t.black + t.blue + t.white + t.green + t.red;
}

int gemTotalWeighted(const Tokens& t) {
    return t.black + t.blue + t.white + t.green + t.red + (2 * t.joker);
}

bool canAfford(const Player& p, const Card& c) {
    if (c.id <= 0) return false;
    Tokens eff = c.getEffectiveCost(p.bonuses);
    int colored_deficit = 0;

    if (p.tokens.black < eff.black) colored_deficit += (eff.black - p.tokens.black);
    if (p.tokens.blue < eff.blue) colored_deficit += (eff.blue - p.tokens.blue);
    if (p.tokens.white < eff.white) colored_deficit += (eff.white - p.tokens.white);
    if (p.tokens.green < eff.green) colored_deficit += (eff.green - p.tokens.green);
    if (p.tokens.red < eff.red) colored_deficit += (eff.red - p.tokens.red);

    return colored_deficit <= p.tokens.joker;
}

int countAffordable(const GameState& state, int pidx) {
    const Player& p = state.players[pidx];
    int count = 0;

    for (size_t i = 0; i < state.faceup_level1.size(); ++i) if (canAfford(p, state.faceup_level1[i])) count++;
    for (size_t i = 0; i < state.faceup_level2.size(); ++i) if (canAfford(p, state.faceup_level2[i])) count++;
    for (size_t i = 0; i < state.faceup_level3.size(); ++i) if (canAfford(p, state.faceup_level3[i])) count++;
    for (size_t i = 0; i < p.reserved.size(); ++i) if (canAfford(p, p.reserved[i])) count++;

    return count;
}

int nobleGapScore(const GameState& state, int pidx) {
    const Player& p = state.players[pidx];
    int total_gap = 0;

    for (size_t i = 0; i < state.available_nobles.size(); ++i) {
        const Noble& n = state.available_nobles[i];
        int gap = 0;
        gap += std::max(0, n.requirements.black - p.bonuses.black);
        gap += std::max(0, n.requirements.blue - p.bonuses.blue);
        gap += std::max(0, n.requirements.white - p.bonuses.white);
        gap += std::max(0, n.requirements.green - p.bonuses.green);
        gap += std::max(0, n.requirements.red - p.bonuses.red);
        total_gap += gap;
    }

    // Lower gap is better, so return negative for direct additive scoring.
    return -total_gap;
}

double cardEfficiency(const Card& c) {
    if (c.id <= 0) return 0.0;
    int required = c.cost.black + c.cost.blue + c.cost.white + c.cost.green + c.cost.red;
    if (required <= 0) return 0.0;

    const double eff = static_cast<double>(c.points) / static_cast<double>(required);
    if (eff >= EFFICIENCY_THRESHOLD) return eff;
    return -(EFFICIENCY_THRESHOLD - eff);
}

double efficiencyScore(const Player& p) {
    double score = 0.0;
    for (size_t i = 0; i < p.cards.size(); ++i) score += cardEfficiency(p.cards[i]);
    for (size_t i = 0; i < p.reserved.size(); ++i) score += cardEfficiency(p.reserved[i]);
    return score;
}

double directionalCommitmentScore(const Player& p) {
    std::vector<Card> high_eff_reserved;
    for (size_t i = 0; i < p.reserved.size(); ++i) {
        const Card& c = p.reserved[i];
        if (c.id <= 0) continue;
        int required = c.cost.black + c.cost.blue + c.cost.white + c.cost.green + c.cost.red;
        if (required <= 0) continue;
        const double eff = static_cast<double>(c.points) / static_cast<double>(required);
        if (eff >= EFFICIENCY_THRESHOLD) high_eff_reserved.push_back(c);
    }

    if (high_eff_reserved.empty()) return 0.0;

    // Demand by color order: black, blue, white, green, red.
    double demand[5] = {0, 0, 0, 0, 0};
    for (size_t i = 0; i < high_eff_reserved.size(); ++i) {
        demand[0] += high_eff_reserved[i].cost.black;
        demand[1] += high_eff_reserved[i].cost.blue;
        demand[2] += high_eff_reserved[i].cost.white;
        demand[3] += high_eff_reserved[i].cost.green;
        demand[4] += high_eff_reserved[i].cost.red;
    }

    double support[5] = {
        static_cast<double>(p.tokens.black + p.bonuses.black) + 0.5 * p.tokens.joker,
        static_cast<double>(p.tokens.blue + p.bonuses.blue) + 0.5 * p.tokens.joker,
        static_cast<double>(p.tokens.white + p.bonuses.white) + 0.5 * p.tokens.joker,
        static_cast<double>(p.tokens.green + p.bonuses.green) + 0.5 * p.tokens.joker,
        static_cast<double>(p.tokens.red + p.bonuses.red) + 0.5 * p.tokens.joker
    };

    int axis = 0;
    double axis_score = -1.0;
    for (int c = 0; c < 5; ++c) {
        double v = demand[c] * (support[c] + 1.0);
        if (v > axis_score) {
            axis_score = v;
            axis = c;
        }
    }

    double focus = 0.0;
    for (size_t i = 0; i < high_eff_reserved.size(); ++i) {
        const Card& c = high_eff_reserved[i];
        double total = static_cast<double>(c.cost.black + c.cost.blue + c.cost.white + c.cost.green + c.cost.red);
        if (total <= 0.0) continue;
        double axis_cost = 0.0;
        if (axis == 0) axis_cost = c.cost.black;
        else if (axis == 1) axis_cost = c.cost.blue;
        else if (axis == 2) axis_cost = c.cost.white;
        else if (axis == 3) axis_cost = c.cost.green;
        else axis_cost = c.cost.red;
        focus += axis_cost / total;
    }
    focus /= static_cast<double>(high_eff_reserved.size());

    double progress = support[axis] / std::max(1.0, demand[axis]);

    double total_demand = demand[0] + demand[1] + demand[2] + demand[3] + demand[4];
    double entropy = 0.0;
    if (total_demand > 0.0) {
        for (int c = 0; c < 5; ++c) {
            if (demand[c] <= 0.0) continue;
            double p_color = demand[c] / total_demand;
            entropy -= p_color * std::log(p_color);
        }
    }
    const double spread = entropy / std::log(5.0);

    const double slot_penalty = std::max(0.0, static_cast<double>(high_eff_reserved.size()) - 2.0);

    return focus + 0.5 * progress - 0.7 * spread - 0.25 * slot_penalty;
}

} // namespace

double evaluate_state(const GameState& state, int root_player, const EvalWeights& w) {
    const int opp = 1 - root_player;
    const Player& self = state.players[root_player];
    const Player& enemy = state.players[opp];

    double score = 0.0;

    score += w.W_POINT_SELF * self.points;
    score -= w.W_POINT_OPP * enemy.points;

    score += w.W_GEM_SELF * gemTotalWeighted(self.tokens);
    score -= w.W_GEM_OPP * gemTotalWeighted(enemy.tokens);

    score += w.W_BONUS_SELF * bonusTotal(self.bonuses);
    score -= w.W_BONUS_OPP * bonusTotal(enemy.bonuses);

    score += w.W_RESERVED_SELF * static_cast<double>(self.reserved.size());
    score -= w.W_RESERVED_OPP * static_cast<double>(enemy.reserved.size());

    score += w.W_NOBLE_PROGRESS_SELF * nobleGapScore(state, root_player);
    score -= w.W_NOBLE_PROGRESS_OPP * nobleGapScore(state, opp);

    score += w.W_AFFORDABLE_SELF * countAffordable(state, root_player);
    score -= w.W_AFFORDABLE_OPP * countAffordable(state, opp);
    score += w.W_EFFICIENCY * (efficiencyScore(self) - efficiencyScore(enemy));
    score += w.W_DIRECTIONAL_COMMITMENT * (directionalCommitmentScore(self) - directionalCommitmentScore(enemy));

    score -= w.W_TURN_PENALTY * state.move_number;

    if (isGameOver(state)) {
        const int winner = determineWinner(state);
        if (winner == root_player) score += w.W_WIN_BONUS;
        else if (winner == opp) score -= w.W_LOSS_PENALTY;
    }

    return score;
}
