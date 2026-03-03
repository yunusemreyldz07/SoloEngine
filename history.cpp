#include "history.h"
#include <cstring>
#include <algorithm>

int historyTable[2][64][64]; // color x fromSquare x toSquare
constexpr int HISTORY_MAX = 16384;

void clear_history() {
    std::memset(historyTable, 0, sizeof(historyTable));
}

void update_history(int color, int fromSq, int toSq, int depth, const Move badQuiets[256], const int& badQuietCount) { 

    int bonus = std::min(10 + 200 * depth, 4096);
    int& bestScore = historyTable[color][fromSq][toSq];

    bestScore += bonus - (bestScore * std::abs(bonus)) / HISTORY_MAX;

    for (int i = 0; i < badQuietCount; ++i) {
        int badFrom = move_from(badQuiets[i]);
        int badTo = move_to(badQuiets[i]);

        if (badFrom == fromSq && badTo == toSq) {
            continue;
        }

        int& badScore = historyTable[color][badFrom][badTo];
        badScore += -bonus - (badScore * std::abs(bonus)) / HISTORY_MAX;
    }
}

int get_history_score(int color, int fromSq, int toSq) {
    return historyTable[color][fromSq][toSq];
}
