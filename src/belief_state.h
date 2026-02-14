#ifndef BELIEF_STATE_H
#define BELIEF_STATE_H

#include "game_logic.h"

#include <random>
#include <vector>

struct BeliefState {
    explicit BeliefState(const std::vector<Card>& all_cards, unsigned int seed = 0);

    void setSeed(unsigned int seed);

    // Produces a sampled fully-instantiated world from the observed state.
    GameState sampleDeterminization(const GameState& observed, int root_player);

private:
    std::mt19937 rng_;
    std::vector<Card> all_cards_;
};

#endif
