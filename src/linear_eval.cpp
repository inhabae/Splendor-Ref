#include "linear_eval.h"

#include <algorithm>
#include <cmath>

namespace {
const double EFFICIENCY_THRESHOLD = 0.24;

struct DirectionalTerms {
    double focus = 0.0;
    double progress = 0.0;
    double spread = 0.0;
    double reserve_match = 0.0;
    double support_match = 0.0;
    double slot_penalty = 0.0;
};

int bonusTotal(const Tokens& t) {
    return t.black + t.blue + t.white + t.green + t.red;
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

DirectionalTerms directionalCommitmentTerms(const Player& p) {
    DirectionalTerms out;
    const int reserved_count = static_cast<int>(p.reserved.size());
    std::vector<Card> high_eff_reserved;
    for (size_t i = 0; i < p.reserved.size(); ++i) {
        const Card& c = p.reserved[i];
        if (c.id <= 0) continue;
        int required = c.cost.black + c.cost.blue + c.cost.white + c.cost.green + c.cost.red;
        if (required <= 0) continue;
        const double eff = static_cast<double>(c.points) / static_cast<double>(required);
        if (eff >= EFFICIENCY_THRESHOLD) high_eff_reserved.push_back(c);
    }

    if (reserved_count >= 1) out.slot_penalty += 0.10;
    if (reserved_count >= 2) out.slot_penalty += 0.25;
    if (reserved_count >= 3) out.slot_penalty += 0.55;

    if (high_eff_reserved.empty()) {
        // Penalize filling reserve slots when reservations are not efficient/committed.
        return out;
    }

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

    // Reward reserve-to-reserve matching: additional reserved cards should align
    // with existing reserved demand rather than opening unrelated directions.
    double pair_similarity = 0.0;
    int pair_count = 0;
    for (size_t i = 0; i < high_eff_reserved.size(); ++i) {
        const Card& a = high_eff_reserved[i];
        const double total_a = static_cast<double>(a.cost.black + a.cost.blue + a.cost.white + a.cost.green + a.cost.red);
        if (total_a <= 0.0) continue;
        const double an[5] = {
            a.cost.black / total_a,
            a.cost.blue / total_a,
            a.cost.white / total_a,
            a.cost.green / total_a,
            a.cost.red / total_a
        };
        for (size_t j = i + 1; j < high_eff_reserved.size(); ++j) {
            const Card& b = high_eff_reserved[j];
            const double total_b = static_cast<double>(b.cost.black + b.cost.blue + b.cost.white + b.cost.green + b.cost.red);
            if (total_b <= 0.0) continue;
            const double bn[5] = {
                b.cost.black / total_b,
                b.cost.blue / total_b,
                b.cost.white / total_b,
                b.cost.green / total_b,
                b.cost.red / total_b
            };
            double dot = 0.0;
            for (int c = 0; c < 5; ++c) dot += an[c] * bn[c];
            pair_similarity += dot;
            pair_count++;
        }
    }
    const double reserve_match = (pair_count > 0) ? (pair_similarity / static_cast<double>(pair_count)) : 0.0;

    // Reward matching reserved demand to current token+bonus support.
    double support_match = 0.0;
    if (total_demand > 0.0) {
        for (int c = 0; c < 5; ++c) {
            const double covered = std::min(demand[c], support[c]);
            support_match += covered;
        }
        support_match /= total_demand;
    }

    // Escalating reserve slot penalty: slight at 1, higher at 2, highest at 3.
    out.focus = focus;
    out.progress = progress;
    out.spread = spread;
    out.reserve_match = reserve_match;
    out.support_match = support_match;
    return out;
}

} // namespace

double evaluate_state(const GameState& state, int root_player, const EvalWeights& w) {
    const int opp = 1 - root_player;
    const Player& self = state.players[root_player];
    const Player& enemy = state.players[opp];

    double score = 0.0;

    score += w.W_POINT_SELF * self.points;
    score -= w.W_POINT_OPP * enemy.points;

    score += w.W_BONUS_SELF * bonusTotal(self.bonuses);
    score -= w.W_BONUS_OPP * bonusTotal(enemy.bonuses);

    score += w.W_RESERVED_SELF * static_cast<double>(self.reserved.size());
    score -= w.W_RESERVED_OPP * static_cast<double>(enemy.reserved.size());

    score += w.W_NOBLE_PROGRESS_SELF * nobleGapScore(state, root_player);
    score -= w.W_NOBLE_PROGRESS_OPP * nobleGapScore(state, opp);

    score += w.W_AFFORDABLE_SELF * countAffordable(state, root_player);
    score -= w.W_AFFORDABLE_OPP * countAffordable(state, opp);
    score += w.W_EFFICIENCY * (efficiencyScore(self) - efficiencyScore(enemy));
    const DirectionalTerms self_dir = directionalCommitmentTerms(self);
    const DirectionalTerms opp_dir = directionalCommitmentTerms(enemy);
    score += w.W_DIR_FOCUS * (self_dir.focus - opp_dir.focus);
    score += w.W_DIR_PROGRESS * (self_dir.progress - opp_dir.progress);
    score -= w.W_DIR_SPREAD * (self_dir.spread - opp_dir.spread);
    score += w.W_DIR_RESERVE_MATCH * (self_dir.reserve_match - opp_dir.reserve_match);
    score += w.W_DIR_SUPPORT_MATCH * (self_dir.support_match - opp_dir.support_match);
    score -= w.W_DIR_SLOT_PENALTY * (self_dir.slot_penalty - opp_dir.slot_penalty);

    score -= w.W_TURN_PENALTY * state.move_number;

    if (isGameOver(state)) {
        const int winner = determineWinner(state);
        if (winner == root_player) score += w.W_WIN_BONUS;
        else if (winner == opp) score -= w.W_LOSS_PENALTY;
    }

    return score;
}
