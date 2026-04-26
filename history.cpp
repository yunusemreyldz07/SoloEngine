#include "history.h"
#include <cstring>
#include <algorithm>

// taking Potential by Eren Araz as reference for corrhist
int CORRHIST_WEIGHT_SCALE = 256;
int CORRHIST_GRAIN = 256;
int CORRHIST_SIZE = 16384;
int CORRHIST_MAX = 16384;

int pawnCorrectionHistory[2][16384];
int historyTable[2][64][64]; // color x fromSquare x toSquare
int conhistTable[12][64][12][64]; // [prevPiece][prevTo][currPiece][currTo]
thread_local MoveInfo moveStack[MAX_PLY];
constexpr int HISTORY_MAX = 16384;

void clear_history() {
    std::memset(historyTable, 0, sizeof(historyTable));
    std::memset(conhistTable, 0, sizeof(conhistTable));
    std::memset(pawnCorrectionHistory, 0, sizeof(pawnCorrectionHistory));
}

void reset_movestack() {
    for (int i = 0; i < MAX_PLY; ++i) {
        moveStack[i] = {-1, -1};
    }
}

static void update_conhist(int piece, int to, int bonus, int ply) {
    constexpr int offsets[] = {1, 2, 4};
    for (int offset : offsets) {
        if (ply >= offset && moveStack[ply - offset].piece >= 0) {
            int prevPiece = moveStack[ply - offset].piece;
            int prevTo = moveStack[ply - offset].to;
            int& entry = conhistTable[prevPiece][prevTo][piece][to];
            entry += bonus - (entry * std::abs(bonus)) / HISTORY_MAX;
        }
    }
}

int get_conhist_score(int piece, int to, int ply) {
    int score = 0;
    constexpr int offsets[] = {1, 2, 4};
    for (int offset : offsets) {
        if (ply >= offset && moveStack[ply - offset].piece >= 0) {
            int prevPiece = moveStack[ply - offset].piece;
            int prevTo = moveStack[ply - offset].to;
            score += conhistTable[prevPiece][prevTo][piece][to];
        }
    }
    return score;
}

void update_history(const Board& board, int color, int fromSq, int toSq, int depth, const Move badQuiets[256], const int& badQuietCount, int ply) { 

    int bonus = std::min(10 + 200 * depth, 4096);

    // Main history update for best move
    int& bestScore = historyTable[color][fromSq][toSq];
    bestScore += bonus - (bestScore * std::abs(bonus)) / HISTORY_MAX;

    // Conhist update for best move
    int movedPiece = board.mailbox[fromSq] - 1;
    update_conhist(movedPiece, toSq, bonus, ply);

    for (int i = 0; i < badQuietCount; ++i) {
        int badFrom = move_from(badQuiets[i]);
        int badTo = move_to(badQuiets[i]);

        if (badFrom == fromSq && badTo == toSq) {
            continue;
        }

        int malus = bonus + (i * 30);

        // Main history malus
        int& badScore = historyTable[color][badFrom][badTo];
        badScore -= malus + (badScore * std::abs(malus)) / HISTORY_MAX;

        // Conhist malus
        int badPiece = board.mailbox[badFrom] - 1;
        update_conhist(badPiece, badTo, -malus, ply);
    }
}

int get_history_score(int color, int fromSq, int toSq) {
    return historyTable[color][fromSq][toSq];
}

void updatePawnCorrectionHistory(Board *board, const int depth, const int diff) {
    uint64_t pawnKey = board->pawnHashTable;
    int entry = pawnCorrectionHistory[board->stm][pawnKey % CORRHIST_SIZE];
    const int scaledDiff = diff * CORRHIST_GRAIN;
    const int newWeight = std::min(depth + 1, 16);
    entry = (entry * (CORRHIST_WEIGHT_SCALE - newWeight) + scaledDiff * newWeight) / CORRHIST_WEIGHT_SCALE;
    entry = std::clamp(entry, -CORRHIST_MAX, CORRHIST_MAX);
    pawnCorrectionHistory[board->stm][pawnKey % CORRHIST_SIZE] = entry;
}

int adjustEvalWithCorrectionHistory(Board *board, const int rawEval) {
    uint64_t pawnKey = board->pawnHashTable;
    int entry = pawnCorrectionHistory[board->stm][pawnKey % CORRHIST_SIZE];
    int mateFound = MATE_SCORE - MAX_PLY;
    return std::clamp(rawEval + entry / CORRHIST_GRAIN, -mateFound + 1, mateFound - 1);
}