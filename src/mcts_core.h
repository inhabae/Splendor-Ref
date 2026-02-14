#ifndef MCTS_CORE_H
#define MCTS_CORE_H

#include "belief_state.h"
#include "game_logic.h"
#include "linear_eval.h"

#include <cstdint>

struct MctsConfig {
    int simulations = 3000;
    double c_puct = 1.25;
    int max_depth = 18;
    int determinizations_per_batch = 8;
    double risk_lambda = 0.30;
    uint64_t seed = 0;
};

Move select_mcts_move(const GameState& state,
                      int root_player,
                      const MctsConfig& cfg,
                      const EvalWeights& weights,
                      BeliefState& belief_state);

#endif
