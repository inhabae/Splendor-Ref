#include "game_logic.h"
#include <vector>
#include <algorithm>
#include <string>
#include <iostream>
#include <random>
#include <ctime>
#include <chrono>

using namespace std;
using namespace std::chrono;

#ifndef ENGINE_TEST
int main() {
    cerr << "Random Engine started" << endl;
    vector<Card> all_c = loadCards("data/cards.json");
    vector<Noble> all_n = loadNobles("data/nobles.json");
    string line; mt19937 rng(time(0));
    while (getline(cin, line)) {
        if (line.empty()) continue;
        try {
            size_t you_p = line.find("\"you\":"); if (you_p == string::npos) continue;
            int you = stoi(line.substr(you_p + 6, line.find_first_of(",}", you_p + 6) - (you_p + 6)));
            size_t act_p = line.find("\"active_player_id\":"); if (act_p == string::npos) continue;
            int active = stoi(line.substr(act_p + 19, line.find_first_of(",}", act_p + 19) - (act_p + 19)));
            
            if (active == you) {
                GameState st = parseJson(line, all_c, all_n);
                vector<Move> mvs = findAllValidMoves(st);
                if (!mvs.empty()) { 
                    uniform_int_distribution<int> d(0, mvs.size() - 1); 
                    string moveStr = moveToString(mvs[d(rng)]);
                    cout << moveStr << endl << flush; 
                } else {
                    cout << "PASS" << endl << flush;
                }
            }
        } catch (const exception& e) {
            cerr << "Random Engine error: " << e.what() << endl;
        } catch (...) {
            cerr << "Random Engine error: unknown" << endl;
        }
    }
    return 0;
}
#endif
