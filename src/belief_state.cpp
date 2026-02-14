#include "belief_state.h"

#include <algorithm>
#include <ctime>
#include <map>
#include <set>

namespace {

bool isConcreteCardId(int id) {
    return id >= 1 && id <= 90;
}

void addKnownCardId(std::set<int>& known, const Card& c) {
    if (isConcreteCardId(c.id)) known.insert(c.id);
}

} // namespace

BeliefState::BeliefState(const std::vector<Card>& all_cards, unsigned int seed)
    : rng_(seed == 0 ? static_cast<unsigned int>(time(nullptr)) : seed),
      all_cards_(all_cards) {}

void BeliefState::setSeed(unsigned int seed) {
    rng_.seed(seed == 0 ? static_cast<unsigned int>(time(nullptr)) : seed);
}

GameState BeliefState::sampleDeterminization(const GameState& observed, int root_player) {
    GameState sampled = observed;

    std::set<int> known_ids;

    for (size_t i = 0; i < sampled.faceup_level1.size(); ++i) addKnownCardId(known_ids, sampled.faceup_level1[i]);
    for (size_t i = 0; i < sampled.faceup_level2.size(); ++i) addKnownCardId(known_ids, sampled.faceup_level2[i]);
    for (size_t i = 0; i < sampled.faceup_level3.size(); ++i) addKnownCardId(known_ids, sampled.faceup_level3[i]);

    for (int p = 0; p < 2; ++p) {
        for (size_t i = 0; i < sampled.players[p].cards.size(); ++i) addKnownCardId(known_ids, sampled.players[p].cards[i]);
        for (size_t i = 0; i < sampled.players[p].reserved.size(); ++i) {
            if (p == (1 - root_player) && sampled.players[p].reserved[i].id >= 91 && sampled.players[p].reserved[i].id <= 93) {
                continue;
            }
            addKnownCardId(known_ids, sampled.players[p].reserved[i]);
        }
    }

    std::map<int, std::vector<Card> > unseen_by_level;
    unseen_by_level[1] = std::vector<Card>();
    unseen_by_level[2] = std::vector<Card>();
    unseen_by_level[3] = std::vector<Card>();

    for (size_t i = 0; i < all_cards_.size(); ++i) {
        const Card& c = all_cards_[i];
        if (!isConcreteCardId(c.id)) continue;
        if (known_ids.count(c.id) > 0) continue;
        unseen_by_level[c.level].push_back(c);
    }

    int opp = 1 - root_player;
    for (size_t i = 0; i < sampled.players[opp].reserved.size(); ++i) {
        Card& hidden = sampled.players[opp].reserved[i];
        if (hidden.id < 91 || hidden.id > 93) continue;

        int level = hidden.id - 90;
        std::vector<Card>& pool = unseen_by_level[level];
        if (pool.empty()) continue;

        std::uniform_int_distribution<int> pick(0, static_cast<int>(pool.size()) - 1);
        int idx = pick(rng_);
        hidden = pool[idx];
        pool[idx] = pool.back();
        pool.pop_back();
    }

    for (int level = 1; level <= 3; ++level) {
        std::vector<Card>& deck = sampled.getDeck(level);
        deck = unseen_by_level[level];
        std::shuffle(deck.begin(), deck.end(), rng_);
    }

    return sampled;
}
