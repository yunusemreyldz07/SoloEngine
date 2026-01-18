#include "history.h"
#include <cstring>
#include <algorithm>

int historyTable[64][64];
Move killerMove[2][100];
uint16_t countinationHistory[12][64][12][64]; // [piece1][toSquare1][piece2][toSquare2]
// history table is basically like "a3 to f3 may be good", "b4 to c4 may be good", etc.
// but countination history is "when my opp plays Nc6, knight to f3 may be good", etc.
// it is like a better version of the history table

int piece_to_index(int piece) {
    // piece is in range -6..6 excluding 0
    if (piece > 0) {
        return piece - 1; // White pieces: 0..5
    } else {
        return (-piece) + 5; // Black pieces: 6..11
    }
}

uint16_t get_countination_score(int piece1, int toSquare1, int piece2, int toSquare2) {
    return countinationHistory[piece1][toSquare1][piece2][toSquare2];
}

uint16_t calculate_countination_score(int ply, int offset, const Move& move, const Move* prevMove) {
    ply -= offset;
    return ply >= 0 ? get_countination_score(piece_to_index(prevMove->piece),
                                             row_col_to_sq(prevMove->toRow, prevMove->toCol),
                                             piece_to_index(move.piece),
                                             row_col_to_sq(move.toRow, move.toCol)) : 0;
}

void clear_countination_history() {
    std::memset(countinationHistory, 0, sizeof(countinationHistory));
}

void update_countination_history(int piece1, int toSquare1, int piece2, int toSquare2, int depth) {
    if (piece1 == 0 || piece2 == 0) {
        return;
    }
    if (toSquare1 < 0 || toSquare1 >= 64 || toSquare2 < 0 || toSquare2 >= 64) {
        return;
    }
    const int idx1 = piece_to_index(piece1);
    const int idx2 = piece_to_index(piece2);
    if (idx1 < 0 || idx1 >= 12 || idx2 < 0 || idx2 >= 12) {
        return;
    }
    // depth*depth because deeper cutoffs are more important
    countinationHistory[idx1][toSquare1][idx2][toSquare2] += depth * depth * 4; // Multiply by 4 to give more weight
}

void clear_history() {
    std::memset(historyTable, 0, sizeof(historyTable));
}

void update_history(int fromSq, int toSq, int depth) {
    // depth*depth because deeper cutoffs are more important
    historyTable[fromSq][toSq] += depth * depth;
}

int get_history_score(int fromSq, int toSq) {
    return historyTable[fromSq][toSq];
}

void add_killer_move(const Move& move, int ply) {
    const bool is_killer0 = (move.fromRow == killerMove[0][ply].fromRow &&
        move.fromCol == killerMove[0][ply].fromCol &&
        move.toRow == killerMove[0][ply].toRow &&
        move.toCol == killerMove[0][ply].toCol);

    const bool is_killer1 = (move.fromRow == killerMove[1][ply].fromRow &&
        move.fromCol == killerMove[1][ply].fromCol &&
        move.toRow == killerMove[1][ply].toRow &&
        move.toCol == killerMove[1][ply].toCol);

    if (is_killer0) {
        return;
    }

    // Promote killer1 to killer0 when it triggers again.
    if (is_killer1) {
        killerMove[1][ply] = killerMove[0][ply];
        killerMove[0][ply] = move;
        return;
    }

    killerMove[1][ply] = killerMove[0][ply];
    killerMove[0][ply] = move;
}

Move get_killer_move(int index, int ply) {
    return killerMove[index][ply];
}

void clear_killer_moves() {
    for (int i = 0; i < 2; ++i) {
        for (int j = 0; j < 100; ++j) {
            killerMove[i][j] = Move(); // Reset to default Move
        }
    }
}
