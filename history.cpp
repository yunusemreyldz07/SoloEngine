#include "history.h"
#include <cstring>
#include <algorithm>

int historyTable[64][64];
Move killerMove[2][100];
int continuationHistory[13][64][13][64];

static inline int piece_index(int piece) {
    return (piece > 0) ? piece : (6 + (-piece));
}

void update_continuation_history(const Move& prevMove, const Move& currMove, int depth) {
    if (prevMove.piece == 0) return;
    int prevPiece = piece_index(prevMove.piece);
    int prevToSq = row_col_to_sq(prevMove.toRow, prevMove.toCol);
    int currPiece = piece_index(currMove.piece);
    int toSq = row_col_to_sq(currMove.toRow, currMove.toCol);
    continuationHistory[prevPiece][prevToSq][currPiece][toSq] += 4 * depth * depth < 10000 ? std::min(4 * depth * depth, 400): 10000;
}

int get_continuation_history_score(const Move& prevMove, const Move& currMove) {
    if (prevMove.piece == 0) return 0;
    int prevPiece = piece_index(prevMove.piece);
    int prevToSq = row_col_to_sq(prevMove.toRow, prevMove.toCol);
    int currPiece = piece_index(currMove.piece);
    int toSq = row_col_to_sq(currMove.toRow, currMove.toCol);
    return continuationHistory[prevPiece][prevToSq][currPiece][toSq];
}

void clear_history() {
    std::memset(historyTable, 0, sizeof(historyTable));
    std::memset(continuationHistory, 0, sizeof(continuationHistory));
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
