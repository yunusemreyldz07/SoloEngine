#include "history.h"
#include <cstring>
#include <algorithm>

int historyTable[64][64];
Move killerMove[2][MAX_PLY];
constexpr int HISTORY_MAX = 16384;

int correctionHistoryWeightScale = 256;
int correctionHistoryGrain = 256;
int correctionHistorySize = 16384;
int correctionHistoryMax = 16384;

int pawnCorrectionHistory[2][16384];

void updatePawnCorrectionHistory(Board& board, const int& depth, const int& diff){
    unsigned long long pawnKey = generatePawnKey(board);
    int entry = pawnCorrectionHistory[board.isWhiteTurn ? 0 : 1][pawnKey % correctionHistorySize];

    const int scaledDiff = diff * correctionHistoryGrain;
    const int newWeight = std::min(depth+1, 16);

    entry = (entry * (correctionHistoryWeightScale - newWeight) + scaledDiff * newWeight) / correctionHistoryWeightScale;

    entry = clamp(entry, -correctionHistoryMax, correctionHistoryMax);

    pawnCorrectionHistory[board.isWhiteTurn ? 0 : 1][pawnKey % correctionHistorySize] = entry;
}

int adjustEvalWithCorrectionHistory(Board& board, int rawEval){
    unsigned long long pawnKey = generatePawnKey(board);
    int entry = pawnCorrectionHistory[board.isWhiteTurn ? 0 : 1][pawnKey % correctionHistorySize];
    int mateFound = MATE_SCORE - MAX_PLY;
    return clamp(rawEval + entry / correctionHistoryGrain, -mateFound + 1, mateFound - 1);
}

void clear_history() {
    std::memset(historyTable, 0, sizeof(historyTable));
    memset(pawnCorrectionHistory, 0, sizeof(pawnCorrectionHistory));
}

void update_history(int fromSq, int toSq, int depth, const Move badQuiets[256], const int& badQuietCount) { 

    int bonus = std::min(10 + 200 * depth, 4096);
    int& bestScore = historyTable[fromSq][toSq];

    bestScore += bonus - (bestScore * std::abs(bonus)) / HISTORY_MAX;

    for (int i = 0; i < badQuietCount; ++i) {
        int badFrom = row_col_to_sq(badQuiets[i].fromRow, badQuiets[i].fromCol);
        int badTo = row_col_to_sq(badQuiets[i].toRow, badQuiets[i].toCol);

        if (badFrom == fromSq && badTo == toSq) {
            continue;
        }

        int malus = bonus + (i * 30);
        int& badScore = historyTable[badFrom][badTo];
        
        badScore -= malus + (badScore * std::abs(malus)) / HISTORY_MAX;
    }
}

int get_history_score(int fromSq, int toSq) {
    return historyTable[fromSq][toSq];
}

void add_killer_move(const Move& move, int ply) {
    if (ply < 0 || ply >= MAX_PLY) return;
    
    if (moves_equal(move, killerMove[0][ply])) {
        return;
    }

    // Promote killer1 to killer0 when it triggers again.
    if (moves_equal(move, killerMove[1][ply])) {
        killerMove[1][ply] = killerMove[0][ply];
        killerMove[0][ply] = move;
        return;
    }

    killerMove[1][ply] = killerMove[0][ply];
    killerMove[0][ply] = move;
}

Move get_killer_move(int index, int ply) {
    if (ply < 0 || ply >= MAX_PLY) return Move();
    return killerMove[index][ply];
}

void clear_killer_moves() {
    for (int i = 0; i < 2; ++i) {
        for (int j = 0; j < MAX_PLY; ++j) {
            killerMove[i][j] = Move(); // Reset to default Move
        }
    }
}
