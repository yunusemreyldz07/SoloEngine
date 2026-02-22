#include "search.h"

#include "bitboard.h"
#include "evaluation.h"

#include <atomic>

#define MAX_MOVES 256

namespace {
std::atomic<bool> stop_search{false};
}

void initLMRtables() {
    // No-op for now.
}

int negamax(Board& board, int depth, int alpha, int beta, int ply) {
    if (stop_search.load(std::memory_order_relaxed)) return 0;

    if (depth <= 0) {
        return evaluate_board(board);
    }

    int moveCount = 0;
    Move moves[MAX_MOVES];
    get_all_moves(board, moves, moveCount);

    if (moveCount == 0) {
        int kingRow = 0, kingCol = 0;
        if (king_square(board, board.stm == WHITE, kingRow, kingCol) &&
            is_square_attacked(board, kingRow, kingCol, board.stm != WHITE)) {
            return -MATE_SCORE + ply;
        }
        return 0;
    }

    int bestEval = -VALUE_INF;
    for (int i = 0; i < moveCount; ++i) {
        board.makeMove(moves[i]);
        int eval = -negamax(board, depth - 1, -beta, -alpha, ply + 1);
        board.unmakeMove(moves[i]);

        if (eval > bestEval) bestEval = eval;
        if (bestEval > alpha) alpha = bestEval;
        if (alpha >= beta) break;
    }

    return bestEval;
}

Move getBestMove(Board& board, int maxDepth, int movetimeMs, const std::vector<uint64_t>& positionHistory, int ply) {
    (void)movetimeMs;
    (void)positionHistory;

    int moveCount = 0;
    Move moves[MAX_MOVES];
    get_all_moves(board, moves, moveCount);
    if (moveCount == 0) return 0;

    Move bestMove = moves[0];
    int alpha = -VALUE_INF;
    int beta = VALUE_INF;

    for (int i = 0; i < moveCount; ++i) {
        board.makeMove(moves[i]);
        int eval = -negamax(board, maxDepth - 1, -beta, -alpha, ply + 1);
        board.unmakeMove(moves[i]);

        if (eval > alpha) {
            alpha = eval;
            bestMove = moves[i];
        }
    }

    return bestMove;
}
