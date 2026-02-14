#ifndef LINEAR_EVAL_H
#define LINEAR_EVAL_H

#include "game_logic.h"

struct EvalWeights {
    // Fixed order for CLI/script compatibility.
    double W_POINT_SELF = 28.866458;
    double W_POINT_OPP = 19.799814;
    double W_BONUS_SELF = 1.2506351;
    double W_BONUS_OPP = 1.443658;
    double W_RESERVED_SELF = 0.0;
    double W_RESERVED_OPP = 0.0;
    double W_NOBLE_PROGRESS_SELF = 0.84936494;
    double W_NOBLE_PROGRESS_OPP = 0.53619669;
    double W_AFFORDABLE_SELF = 0.4;
    double W_AFFORDABLE_OPP = 0.4;
    double W_WIN_BONUS = 761.21519;
    double W_LOSS_PENALTY = 761.21519;
    double W_TURN_PENALTY = 0.020127013;
    double W_EFFICIENCY = 1.4433229;
    double W_DIR_FOCUS = 1.0695103;
    double W_DIR_PROGRESS = 0.53475515;
    double W_DIR_SPREAD = 0.74865721;
    double W_DIR_RESERVE_MATCH = 0.74865721;
    double W_DIR_SUPPORT_MATCH = 0.64170618;
    double W_DIR_SLOT_PENALTY = 1.0695103;
};

double evaluate_state(const GameState& state, int root_player, const EvalWeights& w);

#endif
