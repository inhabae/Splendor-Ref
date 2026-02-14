#ifndef LINEAR_EVAL_H
#define LINEAR_EVAL_H

#include "game_logic.h"

struct EvalWeights {
    // Fixed order for CLI/script compatibility.
    double W_POINT_SELF = 20.0;
    double W_POINT_OPP = 20.0;
    double W_GEM_SELF = 0.25;
    double W_GEM_OPP = 0.25;
    double W_BONUS_SELF = 1.2;
    double W_BONUS_OPP = 1.2;
    double W_RESERVED_SELF = 0.6;
    double W_RESERVED_OPP = 0.6;
    double W_NOBLE_PROGRESS_SELF = 0.9;
    double W_NOBLE_PROGRESS_OPP = 0.9;
    double W_AFFORDABLE_SELF = 0.8;
    double W_AFFORDABLE_OPP = 0.8;
    double W_WIN_BONUS = 1000.0;
    double W_LOSS_PENALTY = 1000.0;
    double W_TURN_PENALTY = 0.01;
    double W_EFFICIENCY = 1.0;
    double W_DIRECTIONAL_COMMITMENT = 1.0;
};

double evaluate_state(const GameState& state, int root_player, const EvalWeights& w);

#endif
