#include "history.h"
#include <cstring>
#include <algorithm>

int historyTable[2][64][64]; // color x fromSquare x toSquare
constexpr int HISTORY_MAX = 16384;
int CORRHIST_WEIGHT_SCALE = 256;
int CORRHIST_GRAIN = 256;
int CORRHIST_SIZE = 16384;
int CORRHIST_MAX = 16384;
int pawnCorrectionHistory[2][16384];

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

        int malus = bonus + (i * 30);
        int& badScore = historyTable[color][badFrom][badTo];
        
        badScore -= malus + (badScore * std::abs(malus)) / HISTORY_MAX;
    }
}

int get_history_score(int color, int fromSq, int toSq) {
    return historyTable[color][fromSq][toSq];
}

uint64_t generatePawnKey(Board *board) {
    uint64_t finalKey = 0ULL;
    uint64_t bitboard;

    bitboard = board->piece[PAWN - 1] & board->color[WHITE];

    while (bitboard) {
        int sq = lsb(bitboard);
        bitboard &= bitboard - 1;
        finalKey ^= (1ULL << sq);
    }

    bitboard = board->piece[PAWN - 1] & board->color[BLACK];

    while (bitboard) {
        int sq = lsb(bitboard);
        bitboard &= bitboard - 1;
        finalKey ^= (1ULL << sq);
    }

    return finalKey;

}

int clamp(int d, int min, int max) {
    const int t = d < min ? min : d;
    return t > max ? max : t;
}

void updatePawnCorrectionHistory(Board *board, const int depth, const int diff) {
    uint64_t pawnKey = generatePawnKey(board);
    int entry = pawnCorrectionHistory[board->stm][pawnKey % CORRHIST_SIZE];
    const int scaledDiff = diff * CORRHIST_GRAIN;
    const int newWeight = std::min(depth + 1, 16);
    entry = (entry * (CORRHIST_WEIGHT_SCALE - newWeight) + scaledDiff * newWeight) / CORRHIST_WEIGHT_SCALE;
    entry = clamp(entry, -CORRHIST_MAX, CORRHIST_MAX);
    pawnCorrectionHistory[board->stm][pawnKey % CORRHIST_SIZE] = entry;
}

int adjustEvalWithCorrectionHistory(Board *board, const int rawEval) {
    uint64_t pawnKey = generatePawnKey(board);
    int entry = pawnCorrectionHistory[board->stm][pawnKey % CORRHIST_SIZE];
    int mateFound = MATE_SCORE - MAX_PLY; // Ensure we don't overflow mate scores
    return clamp(rawEval + entry / CORRHIST_GRAIN, -mateFound + 1, mateFound - 1);
}

void clearPawnCorrectionHistory() {
    std::memset(pawnCorrectionHistory, 0, sizeof(pawnCorrectionHistory));
}