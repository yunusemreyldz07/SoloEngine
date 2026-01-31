#include "history.h"
#include <cstring>
#include <algorithm>

int historyTable[64][64];
Move killerMove[2][100];
int continuationHistory[12][64][12][64];

static const int HISTORY_MAX = 16384;
static const int CH_MAX = 16384;

// Helper: convert piece type (1-6) and color (WHITE=0, BLACK=1) to index 0-11
inline int get_piece_index(int pieceType, int color) {
    if (pieceType == 0) return 0;
    return (pieceType - 1) + (color * 6);
}

// Get continuation history score for a single offset (1, 2, or 4 plies back)
static int get_single_ch_score(const Board& board, int offset, const Move& currentMove) {
    int historySize = static_cast<int>(board.moveHistory.size());
    int prevIdx = historySize - offset;
    if (prevIdx < 0) return 0;

    const Move& prevMove = board.moveHistory[prevIdx];
    if (prevMove.pieceType == 0) return 0;

    // Previous move's color: if current side is white, previous was black
    int prevColor = board.isWhiteTurn ? BLACK : WHITE;
    int prevPieceIdx = get_piece_index(prevMove.pieceType, prevColor);
    int prevToSq = row_col_to_sq(prevMove.toRow, prevMove.toCol);

    // Current move's piece and color
    int currPieceType = currentMove.pieceType;
    if (currPieceType == 0) return 0;
    int currColor = board.isWhiteTurn ? WHITE : BLACK;
    int currPieceIdx = get_piece_index(currPieceType, currColor);
    int currToSq = row_col_to_sq(currentMove.toRow, currentMove.toCol);

    return continuationHistory[prevPieceIdx][prevToSq][currPieceIdx][currToSq];
}

// Get total continuation history score (offsets 1, 2, 4)
int get_continuation_history_score(const Board& board, const Move& currentMove) {
    int score = 0;
    score += get_single_ch_score(board, 1, currentMove);
    score += get_single_ch_score(board, 2, currentMove);
    score += get_single_ch_score(board, 4, currentMove);
    return score;
}

// Get all CH score for gravity calculation: (CH1 + mainHist) / 2 + CH2 + CH4
// Used during scoring (before makeMove)
static int get_all_ch_score(const Board& board, const Move& move, int mainHistScore) {
    int ch1 = get_single_ch_score(board, 1, move);
    int ch2 = get_single_ch_score(board, 2, move);
    int ch4 = get_single_ch_score(board, 4, move);
    return (ch1 + mainHistScore) / 2 + ch2 + ch4;
}

// Get single CH score for update (after makeMove, so offset is adjusted and colors are flipped)
static int get_single_ch_score_for_update(const Board& board, int offset, const Move& currentMove) {
    int historySize = static_cast<int>(board.moveHistory.size());
    int prevIdx = historySize - offset - 1; // -1 because current move is already in history
    if (prevIdx < 0) return 0;

    const Move& prevMove = board.moveHistory[prevIdx];
    if (prevMove.pieceType == 0) return 0;

    // After makeMove, isWhiteTurn is flipped, so we flip the logic
    int prevColor = board.isWhiteTurn ? WHITE : BLACK; // Flipped from scoring
    int prevPieceIdx = get_piece_index(prevMove.pieceType, prevColor);
    int prevToSq = row_col_to_sq(prevMove.toRow, prevMove.toCol);

    int currPieceType = currentMove.pieceType;
    if (currPieceType == 0) return 0;
    int currColor = board.isWhiteTurn ? BLACK : WHITE; // Flipped from scoring
    int currPieceIdx = get_piece_index(currPieceType, currColor);
    int currToSq = row_col_to_sq(currentMove.toRow, currentMove.toCol);

    return continuationHistory[prevPieceIdx][prevToSq][currPieceIdx][currToSq];
}

// Get all CH score for update (after makeMove)
static int get_all_ch_score_for_update(const Board& board, const Move& move, int mainHistScore) {
    int ch1 = get_single_ch_score_for_update(board, 1, move);
    int ch2 = get_single_ch_score_for_update(board, 2, move);
    int ch4 = get_single_ch_score_for_update(board, 4, move);
    return (ch1 + mainHistScore) / 2 + ch2 + ch4;
}

// Update a single CH entry
// Note: This is called AFTER makeMove, so moveHistory already contains the current move
// and isWhiteTurn is flipped
static void update_single_ch(const Board& board, const Move& move, int offset, int bonus, int mainHistScore) {
    int historySize = static_cast<int>(board.moveHistory.size());
    int prevIdx = historySize - offset - 1; // -1 because current move is already in history
    if (prevIdx < 0) return;

    const Move& prevMove = board.moveHistory[prevIdx];
    if (prevMove.pieceType == 0) return;

    // After makeMove, isWhiteTurn is flipped, so we flip the logic
    int prevColor = board.isWhiteTurn ? WHITE : BLACK; // Flipped from scoring
    int prevPieceIdx = get_piece_index(prevMove.pieceType, prevColor);
    int prevToSq = row_col_to_sq(prevMove.toRow, prevMove.toCol);

    int currPieceType = move.pieceType;
    if (currPieceType == 0) return;
    int currColor = board.isWhiteTurn ? BLACK : WHITE; // Flipped from scoring
    int currPieceIdx = get_piece_index(currPieceType, currColor);
    int currToSq = row_col_to_sq(move.toRow, move.toCol);

    int baseScore = get_all_ch_score_for_update(board, move, mainHistScore);
    int scaledBonus = bonus - baseScore * std::abs(bonus) / CH_MAX;

    continuationHistory[prevPieceIdx][prevToSq][currPieceIdx][currToSq] += scaledBonus;
}

// Update all CH entries for a move (offsets 1, 2, 4)
static void update_all_ch(const Board& board, const Move& move, int bonus, int mainHistScore) {
    update_single_ch(board, move, 1, bonus, mainHistScore);
    update_single_ch(board, move, 2, bonus, mainHistScore);
    update_single_ch(board, move, 4, bonus, mainHistScore);
}

// Update continuation history on beta cutoff
void update_continuation_history(const Board& board, const Move& bestMove, int depth, const Move badQuiets[256], int badQuietCount) {
    int bonus = std::min(16 * depth * depth + 32 * depth + 16, 2000);

    int bestFrom = row_col_to_sq(bestMove.fromRow, bestMove.fromCol);
    int bestTo = row_col_to_sq(bestMove.toRow, bestMove.toCol);
    int mainHistScore = historyTable[bestFrom][bestTo];

    update_all_ch(board, bestMove, bonus, mainHistScore);

    for (int i = 0; i < badQuietCount; ++i) {
        const Move& badMove = badQuiets[i];
        // Skip if same as best move
        if (badMove.fromRow == bestMove.fromRow && badMove.fromCol == bestMove.fromCol &&
            badMove.toRow == bestMove.toRow && badMove.toCol == bestMove.toCol) {
            continue;
        }
        int badFrom = row_col_to_sq(badMove.fromRow, badMove.fromCol);
        int badTo = row_col_to_sq(badMove.toRow, badMove.toCol);
        int badMainHistScore = historyTable[badFrom][badTo];
        update_all_ch(board, badMove, -bonus, badMainHistScore);
    }
}

void clear_history() {
    std::memset(historyTable, 0, sizeof(historyTable));
    std::memset(continuationHistory, 0, sizeof(continuationHistory));
    clear_killer_moves();
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
            killerMove[i][j] = Move();
        }
    }
}
