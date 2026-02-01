#include "history.h"
#include <cstring>
#include <algorithm>

int historyTable[64][64];
Move killerMove[2][MAX_PLY];
int conhistTable[6][64][6][64];  // Continuation history table

constexpr int HISTORY_MAX = 16384;

void clear_history() {
    std::memset(historyTable, 0, sizeof(historyTable));
}

void clear_conhist() {
    std::memset(conhistTable, 0, sizeof(conhistTable));
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

// Continuation history update
void update_conhist(const Move& prevMove, const Move& currMove, int depth, const Move badQuiets[256], int badQuietCount) {
    // Validate previous move exists
    if (prevMove.pieceType == 0) return;
    
    int prevPt = prevMove.pieceType - 1;  // 0-5
    int prevTo = prevMove.to_sq();
    int currPt = currMove.pieceType - 1;  // 0-5
    int currTo = currMove.to_sq();
    
    // Bounds check
    if (prevPt < 0 || prevPt > 5 || currPt < 0 || currPt > 5) return;
    if (prevTo < 0 || prevTo > 63 || currTo < 0 || currTo > 63) return;
    
    int bonus = std::min(10 + 200 * depth, 4096);
    int& bestScore = conhistTable[prevPt][prevTo][currPt][currTo];
    
    // Update best move score with gravity formula
    bestScore += bonus - (bestScore * std::abs(bonus)) / HISTORY_MAX;
    
    // Apply malus to bad quiet moves
    for (int i = 0; i < badQuietCount; ++i) {
        int badPt = badQuiets[i].pieceType - 1;
        int badTo = badQuiets[i].to_sq();
        
        if (badPt < 0 || badPt > 5 || badTo < 0 || badTo > 63) continue;
        if (badPt == currPt && badTo == currTo) continue;  // Skip the good move
        
        int malus = bonus + (i * 30);
        int& badScore = conhistTable[prevPt][prevTo][badPt][badTo];
        badScore -= malus + (badScore * std::abs(malus)) / HISTORY_MAX;
    }
}

int get_conhist_score(const Move& prevMove, const Move& currMove) {
    if (prevMove.pieceType == 0) return 0;
    
    int prevPt = prevMove.pieceType - 1;
    int prevTo = prevMove.to_sq();
    int currPt = currMove.pieceType - 1;
    int currTo = currMove.to_sq();
    
    if (prevPt < 0 || prevPt > 5 || currPt < 0 || currPt > 5) return 0;
    if (prevTo < 0 || prevTo > 63 || currTo < 0 || currTo > 63) return 0;
    
    return conhistTable[prevPt][prevTo][currPt][currTo];
}
