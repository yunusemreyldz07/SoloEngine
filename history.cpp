#include "history.h"
#include <cstring>
#include <algorithm>

int historyTable[2][64][64]; // color x fromSquare x toSquare
constexpr int HISTORY_MAX = 16384;

int continuationHistoryTable[7][64][7][64];

void updateContinuationHistory(int oldPiece, int oldToSq, int toPiece, int toSq, int bonus) {
    int& entry = continuationHistoryTable[oldPiece][oldToSq][toPiece][toSq];
    entry += bonus - (entry * std::abs(bonus)) / HISTORY_MAX;
}

int getContinuationHistoryScore(int oldPiece, int oldToSq, int toPiece, int toSq) {
    return continuationHistoryTable[oldPiece][oldToSq][toPiece][toSq];
}

void clear_history() {
    std::memset(historyTable, 0, sizeof(historyTable));
    std::memset(continuationHistoryTable, 0, sizeof(continuationHistoryTable));
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

        int malus = bonus + (i * 30);
        int& badScore = historyTable[color][badFrom][badTo];
        
        badScore -= malus + (badScore * std::abs(malus)) / HISTORY_MAX;
    }
}

int get_history_score(int color, int fromSq, int toSq) {
    return historyTable[color][fromSq][toSq];
}
