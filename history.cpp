#include "history.h"
#include <cstring>
#include <algorithm>

int historyTable[2][64][64]; // color x fromSquare x toSquare
constexpr int HISTORY_MAX = 16384;

int correctionHistoryWeightScale = 256;
int correctionHistoryGrain = 256;
int correctionHistorySize = 16384;
int correctionHistoryMax = 16384;

int pawnCorrectionHistory[2][16384];

void clear_history() {
    std::memset(historyTable, 0, sizeof(historyTable));
    memset(pawnCorrectionHistory, 0, sizeof(pawnCorrectionHistory));
}

void updatePawnCorrectionHistory(Board& board, const int& depth, const int& diff) {
    unsigned long long pawnKey = generatePawnKey(board);
    int entry = pawnCorrectionHistory[board.isWhiteTurn ? 0 : 1][pawnKey % correctionHistorySize];

    const int scaledDiff = diff * correctionHistoryGrain;
    const int newWeight = std::min(depth + 1, 16);

    entry = (entry * (correctionHistoryWeightScale - newWeight) + scaledDiff * newWeight) / correctionHistoryWeightScale;
    entry = clamp(entry, -correctionHistoryMax, correctionHistoryMax);

    pawnCorrectionHistory[board.isWhiteTurn ? 0 : 1][pawnKey % correctionHistorySize] = entry;
}

int adjustEvalWithCorrectionHistory(Board& board, int rawEval) {
    unsigned long long pawnKey = generatePawnKey(board);
    int entry = pawnCorrectionHistory[board.isWhiteTurn ? 0 : 1][pawnKey % correctionHistorySize];
    int mateFound = MATE_SCORE - MAX_PLY;
    return clamp(rawEval + entry / correctionHistoryGrain, -mateFound + 1, mateFound - 1);
}

void update_history(int color, int fromSq, int toSq, int depth, const Move badQuiets[256], const int& badQuietCount) { 

    int bonus = std::min(10 + 200 * depth, 4096);
    int& bestScore = historyTable[color][fromSq][toSq];

    bestScore += bonus - (bestScore * std::abs(bonus)) / HISTORY_MAX;

    for (int i = 0; i < badQuietCount; ++i) {
        int badFrom = row_col_to_sq(badQuiets[i].fromRow, badQuiets[i].fromCol);
        int badTo = row_col_to_sq(badQuiets[i].toRow, badQuiets[i].toCol);

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
