#include "history.h"
#include "board.h"
#include <cstring>
#include <algorithm>

int historyTable[64][64];
Move killerMove[2][100];
int HISTORY_MAX = 16384;

int continuationHistory[12][64][12][64]; 
const int CH_MAX = 16384;

inline bool is_same_move(const Move& a, const Move& b) {
    return a.fromRow == b.fromRow && a.fromCol == b.fromCol &&
           a.toRow == b.toRow && a.toCol == b.toCol;
}

inline int get_piece_index(int piece, int color) {
    if (piece == 0) return 0;
    return (piece - 1) + (color * 6);
}

inline Move get_last_move(const Board& board) {
    if (board.moveHistory.empty()) {
        return Move();
    }
    return board.moveHistory.back();
}

void clear_history() {
    std::memset(historyTable, 0, sizeof(historyTable));
    std::memset(continuationHistory, 0, sizeof(continuationHistory));
    clear_killer_moves();
}

int get_continuation_history_score(const Board& board, const Move& currentMove) {
    if (board.moveHistory.empty()) return 0;

    const Move& prevMove = get_last_move(board);

    int prevToSq = row_col_to_sq(prevMove.toRow, prevMove.toCol);
    int prevPieceType = prevMove.pieceType;
    if (prevPieceType == 0) return 0;

    int prevColor = !board.isWhiteTurn ? WHITE : BLACK;
    int prevIdx = get_piece_index(prevPieceType, prevColor);

    int currFromSq = row_col_to_sq(currentMove.fromRow, currentMove.fromCol);
    int currToSq = row_col_to_sq(currentMove.toRow, currentMove.toCol);
    int currPieceType = currentMove.pieceType;
    if (currPieceType == 0) return 0;
    int currColor = board.isWhiteTurn ? WHITE : BLACK;
    int currIdx = get_piece_index(currPieceType, currColor);

    return continuationHistory[prevIdx][prevToSq][currIdx][currToSq];
}

void update_continuation_history(const Board& board, const Move& move, int depth, const Move badQuiets[256], const int& badQuietCount) {
    if (board.moveHistory.empty()) return;

    const Move& prevMove = board.moveHistory.back();
    int prevToSq = row_col_to_sq(prevMove.toRow, prevMove.toCol);
    int prevPieceType = prevMove.pieceType;
    if (prevPieceType == 0) return;

    int prevColor = !board.isWhiteTurn ? WHITE : BLACK;
    int prevIdx = get_piece_index(prevPieceType, prevColor);

    int moveFromSq = row_col_to_sq(move.fromRow, move.fromCol);
    int moveToSq = row_col_to_sq(move.toRow, move.toCol);
    int movePieceType = move.pieceType;
    if (movePieceType == 0) return;
    int moveColor = board.isWhiteTurn ? WHITE : BLACK;
    int moveIdx = get_piece_index(movePieceType, moveColor);
    int bonus = std::min(16 * depth * depth + 32 * depth + 16, 2000); 
    int& entry = continuationHistory[prevIdx][prevToSq][moveIdx][moveToSq];
    entry += bonus - (entry * std::abs(bonus)) / CH_MAX;
    
    // Penalize bad quiet moves in continuation history
    for (int i = 0; i < badQuietCount; ++i) {
        int badFromSq = row_col_to_sq(badQuiets[i].fromRow, badQuiets[i].fromCol);
        int badToSq = row_col_to_sq(badQuiets[i].toRow, badQuiets[i].toCol);
        int badPieceType = badQuiets[i].pieceType;
        if (badPieceType == 0) continue;
        
        // Skip if this is the same move that caused the beta cutoff
        if (badFromSq == moveFromSq && badToSq == moveToSq) {
            continue;
        }
        
        int badColor = board.isWhiteTurn ? WHITE : BLACK;
        int badIdx = get_piece_index(badPieceType, badColor);
        
        // Progressive penalty - moves tried later get penalized more
        int malus = bonus + (i * 30);
        int& badEntry = continuationHistory[prevIdx][prevToSq][badIdx][badToSq];
        badEntry -= malus + (badEntry * std::abs(malus)) / CH_MAX;
    }
}

void update_history(int fromSq, int toSq, int depth, const Move badQuiets[256], const int& badQuietCount) {
    const int HISTORY_MAX = 16384; 

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
