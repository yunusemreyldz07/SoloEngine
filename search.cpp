#include "search.h"
#include "evaluation.h"
#include <algorithm>
#include <limits>
#include <cstdlib>
#include <iostream>
int scoreMove(const Board& board, const Move& move) {
    if (move.capturedPiece != 0 || move.isEnPassant) {
        int victimPiece = move.isEnPassant ? pawn : std::abs(move.capturedPiece);
        int victimValue = PIECE_VALUES[victimPiece];

        int attackerPiece = board.squares[move.fromRow][move.fromCol];
        int attackerValue = PIECE_VALUES[std::abs(attackerPiece)];

        return 10000 + (victimValue * 10) - attackerValue;
    }

    if (move.promotion != 0) {
        return 9000;
    }

    if (move.isCastling) {
        return 5000;
    }

    return 0;
}

int quiescence(Board& board, int alpha, int beta, int ply, std::vector<uint64_t>& history) {
    if (is_threefold_repetition(history)) {
        return repetition_draw_score(board);
    }

    // Stand-pat evaluation: current position score from current player's perspective
    int standPat = evaulate_board(board);
    
    // In negamax, we need to negate the evaluation if it's black's turn
    if (!board.isWhiteTurn) {
        standPat = -standPat;
    }

    if (standPat >= beta) {
        return beta;
    }
    if (standPat > alpha) {
        alpha = standPat;
    }

    std::vector<Move> moves = get_capture_moves(board);
    std::sort(moves.begin(), moves.end(), [&](const Move& a, const Move& b) {
        return scoreMove(board, a) > scoreMove(board, b);
    });

    for (Move& move : moves) {
        board.makeMove(move);
        history.push_back(position_key(board));

        // Reject illegal moves that leave our own king in check
        bool sideJustMovedWasWhite = !board.isWhiteTurn;
        int kingRow = sideJustMovedWasWhite ? board.whiteKingRow : board.blackKingRow;
        int kingCol = sideJustMovedWasWhite ? board.whiteKingCol : board.blackKingCol;
        if (is_square_attacked(board, kingRow, kingCol, board.isWhiteTurn)) {
            history.pop_back();
            board.unmakeMove(move);
            continue;
        }

        // Negamax: negate the score from opponent's perspective
        int score = -quiescence(board, -beta, -alpha, ply + 1, history);
        history.pop_back();
        board.unmakeMove(move);

        if (score >= beta) {
            return beta;
        }
        if (score > alpha) {
            alpha = score;
        }
    }

    return alpha;
}

// Replacing minimax with negamax version for simplicity
int negamax(Board& board, int depth, int alpha, int beta, int ply, std::vector<uint64_t>& history) {
    if (is_threefold_repetition(history)) {
        return repetition_draw_score(board);
    }

    if (depth == 0) {
        return quiescence(board, alpha, beta, ply, history);
    }

    bool whiteToMove = board.isWhiteTurn;
    std::vector<Move> possibleMoves = get_all_moves(board, whiteToMove);
    
    std::sort(possibleMoves.begin(), possibleMoves.end(), [&](const Move& a, const Move& b) {
        return scoreMove(board, a) > scoreMove(board, b);
    });

    constexpr int MATE_SCORE = 100000;
    int legalMoveCount = 0;
    int maxEval = std::numeric_limits<int>::min() / 2;

    for (Move& move : possibleMoves) { // Moves that leave king in check are skipped
        board.makeMove(move);
        history.push_back(position_key(board));

        int kingRow = whiteToMove ? board.whiteKingRow : board.blackKingRow;
        int kingCol = whiteToMove ? board.whiteKingCol : board.blackKingCol;
        if (is_square_attacked(board, kingRow, kingCol, !whiteToMove)) {
            history.pop_back();
            board.unmakeMove(move);
            continue;
        }

        legalMoveCount++;
        int eval = -negamax(board, depth - 1, -beta, -alpha, ply + 1, history);
        history.pop_back();
        board.unmakeMove(move);

        maxEval = std::max(maxEval, eval);
        alpha = std::max(alpha, eval);
        if (beta <= alpha) break; // beta cutoff
    }

    if (legalMoveCount == 0) {
        int kingRow = whiteToMove ? board.whiteKingRow : board.blackKingRow;
        int kingCol = whiteToMove ? board.whiteKingCol : board.blackKingCol;
        if (is_square_attacked(board, kingRow, kingCol, !whiteToMove)) {
            return -MATE_SCORE + ply;
        } else {
            return 0;
        }
    }
    return maxEval;
}


Move getBestMove(Board& board, int depth, const std::vector<uint64_t>& baseHistory) {
    bool isWhite = board.isWhiteTurn;
    std::vector<Move> possibleMoves = get_all_moves(board, isWhite);
    int kingRow = isWhite ? board.whiteKingRow : board.blackKingRow;
    int kingCol = isWhite ? board.whiteKingCol : board.blackKingCol;
    std::sort(possibleMoves.begin(), possibleMoves.end(), [&](const Move& a, const Move& b) {
        return scoreMove(board, a) > scoreMove(board, b);
    });
    
    Move bestMove;
    int bestValue = isWhite ? std::numeric_limits<int>::min() / 2 : std::numeric_limits<int>::max() / 2;
    if(!possibleMoves.empty()) bestMove = possibleMoves[0]; 

    std::vector<uint64_t> history = baseHistory;
    if (history.empty()) history.push_back(position_key(board));

    for (Move move : possibleMoves) {
        board.makeMove(move);
        history.push_back(position_key(board));

        int newKingRow = isWhite ? board.whiteKingRow : board.blackKingRow;
        int newKingCol = isWhite ? board.whiteKingCol : board.blackKingCol;
        if (is_square_attacked(board, newKingRow, newKingCol, !isWhite)) {
            history.pop_back();
            board.unmakeMove(move);
            continue;
        }

        int val = -negamax(board, depth - 1, -2000000000, 2000000000, 1, history);
        history.pop_back();
        board.unmakeMove(move);
        
        if (val > bestValue) {
        bestValue = val;
        bestMove = move;
        std::cout << "info score cp " << bestValue << " depth " << depth << std::endl;
        }
    }
    return bestMove;
}