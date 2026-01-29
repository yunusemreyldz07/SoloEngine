#include "history.h"
#include <cstring>
#include <algorithm>

int historyTable[64][64];
Move killerMove[2][100];
int HISTORY_MAX = 16384;
// continuationHistory[previousPiece][previousTargetSq][currentPiece][currentTargetSq]
int continuationHistory[12][64][12][64];

inline int get_piece_index(int piece, int color) {
    const int p = std::abs(piece);
    if (p < 1 || p > 6) return 0; // guard invalid piece ids
    return (p - 1) + (color * 6);
}

inline bool is_same_move(const Move& a, const Move& b) {
    return a.fromRow == b.fromRow && a.fromCol == b.fromCol &&
           a.toRow == b.toRow && a.toCol == b.toCol;
}

inline bool is_quiet_move(const Move& m) {
    return m.capturedPiece == 0 && !m.isEnPassant && m.promotion == 0;
}

void clear_history() {
    std::memset(historyTable, 0, sizeof(historyTable));
    std::memset(continuationHistory, 0, sizeof(continuationHistory));
}

Move get_prev_move(const Board& board) {
    if (board.moveHistory.empty()) {
        return Move(); // Return empty move
    }
    return board.moveHistory.back();
}

void update_single_ch(int prevPieceIdx, int prevToSq, int currPieceIdx, int currToSq, int bonus) {
    int& entry = continuationHistory[prevPieceIdx][prevToSq][currPieceIdx][currToSq];
    entry += bonus - (entry * std::abs(bonus)) / HISTORY_MAX;
}

int get_continuation_history_score(const Board& board, const Move& currentMove) {
    Move prevMove = get_prev_move(board);

    // if no prev move then return 0
    if (prevMove.fromRow == 0 && prevMove.fromCol == 0 && prevMove.toRow == 0 && prevMove.toCol == 0) {
        return 0;
    }
    if (!is_quiet_move(prevMove) || !is_quiet_move(currentMove)) {
        return 0;
    }
    int prevToSq = row_col_to_sq(prevMove.toRow, prevMove.toCol);
    int prevPieceType = prevMove.pieceType;
    if (prevPieceType == 0) return 0;

    // derive color from signed piece for safety
    int prevColor = (prevPieceType > 0) ? WHITE : BLACK;
    
    // find current move details
    int currFromSq = row_col_to_sq(currentMove.fromRow, currentMove.fromCol);
    int currToSq = row_col_to_sq(currentMove.toRow, currentMove.toCol);
    int currPieceType = board.mailbox[currFromSq];
    if (currPieceType == 0) return 0;
    int currColor = (currPieceType > 0) ? WHITE : BLACK;

    // calc indexes
    int prevIdx = get_piece_index(prevPieceType, prevColor);
    int currIdx = get_piece_index(currPieceType, currColor);

    return continuationHistory[prevIdx][prevToSq][currIdx][currToSq];
}

void update_continuation_history(const Board& board, const Move& move, int depth, const Move badQuiets[], int badQuietCount) {
    Move prevMove = get_prev_move(board);
    if (prevMove.fromRow == 0 && prevMove.fromCol == 0 && prevMove.toRow == 0 && prevMove.toCol == 0) {
        return;
    }
    if (!is_quiet_move(prevMove) || !is_quiet_move(move)) {
        return;
    }
    if (depth < 2) {
        return;
    }

    // Latest move info
    int prevToSq = row_col_to_sq(prevMove.toRow, prevMove.toCol);
    int prevPieceType = prevMove.pieceType;
    if (prevPieceType == 0) return;
    int prevColor = (prevPieceType > 0) ? WHITE : BLACK;
    int prevIdx = get_piece_index(prevPieceType, prevColor);

    // Current best move (to be rewarded)
    int moveToSq = row_col_to_sq(move.toRow, move.toCol);
    int moveFromSq = row_col_to_sq(move.fromRow, move.fromCol);
    int movePieceType = board.mailbox[moveFromSq];
    if (movePieceType == 0) return;
    int moveColor = (movePieceType > 0) ? WHITE : BLACK;
    int moveIdx = get_piece_index(movePieceType, moveColor);
    // Bonus calculation
    int bonus = 8 * depth * depth + 16 * depth + 8;

    // Reward the best move
    update_single_ch(prevIdx, prevToSq, moveIdx, moveToSq, bonus);

    // Punish bad quiet moves
    for (int i = 0; i < badQuietCount; ++i) {
        const Move& badMove = badQuiets[i];
        
        if (is_same_move(badMove, move)) continue;
        if (!is_quiet_move(badMove)) continue;

        int badFromSq = row_col_to_sq(badMove.fromRow, badMove.fromCol);
        int badToSq = row_col_to_sq(badMove.toRow, badMove.toCol);
        int badPieceType = board.mailbox[badFromSq];
        if (badPieceType == 0) continue;
        int badColor = (badPieceType > 0) ? WHITE : BLACK;
        int badIdx = get_piece_index(badPieceType, badColor); // the one who made the bad move is also us

        // And... Thy punishment is DEATH (Ultrakill reference)
        update_single_ch(prevIdx, prevToSq, badIdx, badToSq, -(bonus / 2));
    }
}

void update_history(int fromSq, int toSq, int depth, const Move badQuiets[64], const int& badQuietCount) {
    const int HISTORY_MAX = 16384; 

    int bonus = std::min(10 + 200 * depth, 4096);
    int& bestScore = historyTable[fromSq][toSq];

    bestScore += bonus - (bestScore * std::abs(bonus)) / HISTORY_MAX;

    for (int i = 0; i < badQuietCount; ++i) {
        if (i >= 64) break;

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
