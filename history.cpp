#include "history.h"
#include <cstring>
#include <algorithm>

int historyTable[2][64][64]; // [color][fromSq][toSq] 0 is white, 1 is black
Move killerMove[2][2][100]; // [color][index][ply] 0 is white, 1 is black
const int HISTORY_MAX = 16384;

void clear_history() {
    std::memset(historyTable, 0, sizeof(historyTable));
}

void update_history(int color, int fromSq, int toSq, int depth, const Move badQuiets[64], const int& badQuietCount) {
    const int HISTORY_MAX = 16384; 

    int bonus = std::min(10 + 200 * depth, 4096);
    int& bestScore = historyTable[color][fromSq][toSq];
    bestScore += bonus - (bestScore * std::abs(bonus)) / HISTORY_MAX;

    for (int i = 0; i < badQuietCount; ++i) {
        if (i >= 64) break;

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

void add_killer_move(int color, const Move& move, int ply) {
    const bool is_killer0 = (move.fromRow == killerMove[color][0][ply].fromRow &&
        move.fromCol == killerMove[color][0][ply].fromCol &&
        move.toRow == killerMove[color][0][ply].toRow &&
        move.toCol == killerMove[color][0][ply].toCol);

    const bool is_killer1 = (move.fromRow == killerMove[color][1][ply].fromRow &&
        move.fromCol == killerMove[color][1][ply].fromCol &&
        move.toRow == killerMove[color][1][ply].toRow &&
        move.toCol == killerMove[color][1][ply].toCol);

    if (is_killer0) {
        return;
    }

    // Promote killer1 to killer0 when it triggers again.
    if (is_killer1) {
        killerMove[color][1][ply] = killerMove[color][0][ply];
        killerMove[color][0][ply] = move;
        return;
    }

    killerMove[color][1][ply] = killerMove[color][0][ply];
    killerMove[color][0][ply] = move;
}

Move get_killer_move(int color, int index, int ply) {
    return killerMove[color][index][ply];
}

void clear_killer_moves() {
    for (int i = 0; i < 2; ++i) {
        for (int j = 0; j < 2; ++j) {
            for (int k = 0; k < 100; ++k) {
                killerMove[i][j][k] = Move(); // Reset to default Move
            }
        }
    }
}
