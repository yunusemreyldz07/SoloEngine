#include "search.h"
#include "evaluation.h"
#include "board.h"
#include <algorithm>
#include <chrono>
#include <limits>
#include <cstdlib>
#include <iostream>

// Mate score used across search functions
constexpr int MATE_SCORE = 100000;

static std::chrono::steady_clock::time_point gSearchStart;
static int gTimeLimitMs = std::numeric_limits<int>::max();
static bool gTimeLimited = false;

static const char columns[] = "abcdefgh";

Move killerMove[2][100];
int historyTable[64][64];

int scoreMove(const Board& board, const Move& move, int ply) {
    int moveScore = 0;
    int from = move.fromRow * 8 + move.fromCol;
    int to = move.toRow * 8 + move.toCol;
    int historyScore = historyTable[from][to];
    moveScore += historyScore;
    if (move.capturedPiece != 0 || move.isEnPassant) {
        int victimPiece = move.isEnPassant ? pawn : std::abs(move.capturedPiece);
        int victimValue = PIECE_VALUES[victimPiece];

        int attackerPiece = board.squares[move.fromRow][move.fromCol];
        int attackerValue = PIECE_VALUES[std::abs(attackerPiece)];
        moveScore += 10000 + (victimValue * 10) - attackerValue;

        return moveScore;
    }

    if (ply >= 0 && move.fromCol == killerMove[0][ply].fromCol && move.fromRow == killerMove[0][ply].fromRow &&
        move.toCol == killerMove[0][ply].toCol && move.toRow == killerMove[0][ply].toRow) {
        moveScore += 8000;
        return moveScore;
    }

    if (ply >= 0 && move.fromCol == killerMove[1][ply].fromCol && move.fromRow == killerMove[1][ply].fromRow &&
        move.toCol == killerMove[1][ply].toCol && move.toRow == killerMove[1][ply].toRow) {
        moveScore += 7000;
        return moveScore;
    }

    if (move.promotion != 0) {
        moveScore += 9000;
        return moveScore;
    }

    if (move.isCastling) {
        // Castling is good for king safety, but keep the bonus modest so we don't prefer it over
        // urgent defensive moves (like saving a hanging piece) at shallow depth.
        moveScore += 500;
        return moveScore;
    }

    return 0;
}

static std::string move_to_uci(const Move& m) {
    std::string s;
    s += columns[m.fromCol];
    s += static_cast<char>('0' + (8 - m.fromRow));
    s += columns[m.toCol];
    s += static_cast<char>('0' + (8 - m.toRow));
    if (m.promotion != 0) {
        switch (std::abs(m.promotion)) {
            case queen: s += 'q'; break;
            case rook: s += 'r'; break;
            case bishop: s += 'b'; break;
            case knight: s += 'n'; break;
            default: break;
        }
    }
    return s;
}

int quiescence(Board& board, int alpha, int beta, int ply, std::vector<uint64_t>& history) {
    if (gTimeLimited) {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - gSearchStart).count();
        if (elapsed >= gTimeLimitMs) {
            int standPat = evaluate_board(board);
            if (!board.isWhiteTurn) standPat = -standPat;
            return standPat;
        }
    }

    if (is_threefold_repetition(history)) {
        return repetition_draw_score(board);
    }

    // Stand-pat evaluation: current position score from current player's perspective
    int standPat = evaluate_board(board);
    
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
        return scoreMove(board, a, ply) > scoreMove(board, b, ply);
    });

    for (Move& move : moves) {
        int victimValue = move.isEnPassant ? 100 : PIECE_VALUES[abs(move.capturedPiece)];
        if (standPat + victimValue + 200 < alpha) {
            continue; // not worth calculating
        }
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
int negamax(Board& board, int depth, int alpha, int beta, int ply, std::vector<uint64_t>& history, std::vector<Move>& pvLine) {
    if (gTimeLimited) {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - gSearchStart).count();
        if (elapsed >= gTimeLimitMs) {
            pvLine.clear();
            int standPat = evaluate_board(board);
            if (!board.isWhiteTurn) standPat = -standPat;
            return standPat;
        }
    }

    if (is_threefold_repetition(history)) {
        return repetition_draw_score(board);
    }

    if (depth == 0) {
        pvLine.clear();
        return quiescence(board, alpha, beta, ply, history);
    }
    uint64_t currentHash = position_key(board);

    TTEntry* ttEntry = probeTranspositionTable(currentHash, transpositionTable);
    if (ttEntry != nullptr && ttEntry->depth >= depth) {
        if (ttEntry->flag == EXACT) {
            pvLine.clear();
            if (ttEntry->bestMove.fromRow != 0 || ttEntry->bestMove.toRow != 0 || ttEntry->bestMove.fromCol != 0 || ttEntry->bestMove.toCol != 0) {
                pvLine.push_back(ttEntry->bestMove);
            }
            return ttEntry->score;  // Exact score
        }
        if (ttEntry->flag == ALPHA && ttEntry->score <= alpha) {
            return alpha;
        }
        if (ttEntry->flag == BETA && ttEntry->score >= beta) {
            return beta;
        }
    }

    bool whiteToMove = board.isWhiteTurn;
    std::vector<Move> possibleMoves = get_all_moves(board, whiteToMove);
    
    std::sort(possibleMoves.begin(), possibleMoves.end(), [&](const Move& a, const Move& b) {
        return scoreMove(board, a, ply) > scoreMove(board, b, ply);
    });

    const int alphaOrig = alpha;
    int legalMoveCount = 0;
    int maxEval = std::numeric_limits<int>::min() / 2;
    Move bestMove{};
    pvLine.clear();

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
        // Futility pruning removed for quiet moves at low depth: it was cutting defensive moves
        // (like retreating a hanging piece) and led to nonsensical choices such as castling while
        // dropping material. Leaving the node unpruned keeps safety-first replies available.
        std::vector<Move> childPv;
        int eval;
        bool inCheck = is_square_attacked(board, kingRow, kingCol, !whiteToMove);
        bool fullDepthSearch = true;

        // depth > 2, no check, many legal moves, non-capture, non-promotion. (conditions for Late Move Reduction)
        if (depth >= 3 && inCheck == false && legalMoveCount > 4 && move.capturedPiece == 0 && move.promotion == 0) {
            
            int reduction = 1;
            if (legalMoveCount > 10) reduction = 2; // if move is late enough, reduce more 
            if (depth > 8 && legalMoveCount > 20) reduction = 3; 

            // Perform reduced-depth search 
            eval = -negamax(board, depth - 1 - reduction, -beta, -alpha, ply + 1, history, childPv);

            // if eval is higher than alpha, then the result is interesting enough to research at full depth
            if (eval > alpha) {
                fullDepthSearch = true; 
            } else {
                fullDepthSearch = false; // LMR result is sufficient, no need to re-search
            }
        }

        if (fullDepthSearch) {
             eval = -negamax(board, depth - 1, -beta, -alpha, ply + 1, history, childPv);
        }

        history.pop_back();
        board.unmakeMove(move);

        if (eval > maxEval) {
            maxEval = eval;
            bestMove = move;
            pvLine.clear();
            pvLine.push_back(move);
            pvLine.insert(pvLine.end(), childPv.begin(), childPv.end());
        }
        alpha = std::max(alpha, eval);
        if (beta <= alpha){
            if (move.capturedPiece == 0 && !move.isEnPassant) {
                killerMove[1][ply] = killerMove[0][ply];
                killerMove[0][ply] = move;
            }
            int bonus = depth * depth;
            int from = move.fromRow * 8 + move.fromCol;
            int to = move.toRow * 8 + move.toCol;
            historyTable[from][to] += bonus;
            break; // beta cutoff
        }
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
    TTFlag flag;
    if (maxEval <= alphaOrig) {
        flag = ALPHA;  // Fail-low
    } else if (maxEval >= beta) {
        flag = BETA;   // Fail-high
    } else {
        flag = EXACT;  // PV node
    }
    
    storeInTT(currentHash, maxEval, depth, flag, bestMove, transpositionTable);
    
    return maxEval;
}

Move getBestMove(Board& board, int maxDepth, const std::vector<uint64_t>& baseHistory, int movetimeMs) {
    bool isWhite = board.isWhiteTurn;
    std::vector<Move> possibleMoves = get_all_moves(board, isWhite);
    std::sort(possibleMoves.begin(), possibleMoves.end(), [&](const Move& a, const Move& b) {
        return scoreMove(board, a, 0) > scoreMove(board, b, 0);
    });

    if (possibleMoves.empty()) return {};
    if (possibleMoves.size() == 1) return possibleMoves[0];

    Move bestMoveSoFar = possibleMoves[0]; 

    std::vector<uint64_t> history = baseHistory;
    if (history.empty()) history.push_back(position_key(board));

    gSearchStart = std::chrono::steady_clock::now();
    gTimeLimited = (movetimeMs > 0);
    gTimeLimitMs = gTimeLimited ? movetimeMs : std::numeric_limits<int>::max();
    const int effectiveMaxDepth = gTimeLimited ? 128 : maxDepth;

    bool timeExpired = false;

    for (int depth = 1; depth <= effectiveMaxDepth; depth++) {
        
        if (gTimeLimited) {
            auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::milliseconds>(now - gSearchStart).count() >= gTimeLimitMs) {
                break;
            }
        }

        int bestValue = std::numeric_limits<int>::min() / 2;
        int alpha = -2000000000;
        int beta = 2000000000;
        
        Move currentDepthBestMove; // Only the best move found at this depth
        bool thisDepthCompleted = true; // Is this depth completed?

        // PV Move Ordering (Put the best move from the previous depth first)
        if (depth > 1) {
             // Here we use bestMoveSoFar because it was the winner of the previous depth.
             Move pvMove = bestMoveSoFar; 
             std::sort(possibleMoves.begin(), possibleMoves.end(), [&](const Move& a, const Move& b) {
                bool aIsPV = (a.fromRow == pvMove.fromRow && a.fromCol == pvMove.fromCol && a.toRow == pvMove.toRow && a.toCol == pvMove.toCol);
                bool bIsPV = (b.fromRow == pvMove.fromRow && b.fromCol == pvMove.fromCol && b.toRow == pvMove.toRow && b.toCol == pvMove.toCol);
                if (aIsPV) return true;
                if (bIsPV) return false;
                return scoreMove(board, a, 0) > scoreMove(board, b, 0);
            });
        }

        for (Move move : possibleMoves) {
            // In-loop time check
            if (gTimeLimited) {
                auto nowInner = std::chrono::steady_clock::now();
                if (std::chrono::duration_cast<std::chrono::milliseconds>(nowInner - gSearchStart).count() >= gTimeLimitMs) {
                    timeExpired = true;
                    thisDepthCompleted = false; // Depth incomplete
                    break; 
                }
            }

            board.makeMove(move);
            history.push_back(position_key(board));

            // Check if the king is in check, etc
            int newKingRow = isWhite ? board.whiteKingRow : board.blackKingRow;
            int newKingCol = isWhite ? board.whiteKingCol : board.blackKingCol;
            if (is_square_attacked(board, newKingRow, newKingCol, !isWhite)) {
                history.pop_back();
                board.unmakeMove(move);
                continue;
            }

            std::vector<Move> childPv;
            int val = -negamax(board, depth - 1, -beta, -alpha, 1, history, childPv);
            
            history.pop_back();
            board.unmakeMove(move);

            if (val > bestValue) {
                bestValue = val;
                currentDepthBestMove = move;
                
                if (bestValue > alpha) {
                    alpha = bestValue;
                }
                
                if (thisDepthCompleted) { 
                    std::cout << "info depth " << depth << " score cp " << bestValue << " pv ";
                    std::cout << move_to_uci(move) << " ";
                    for (const Move& pvMove : childPv) {
                        std::cout << move_to_uci(pvMove) << " ";
                    }
                    std::cout << std::endl;
                }
            }
        }

        if (timeExpired) {
            break; 
        }

        if (thisDepthCompleted && (bestValue > std::numeric_limits<int>::min() / 2)) {
            bestMoveSoFar = currentDepthBestMove;
        }

        if (bestValue >= MATE_SCORE - 50) {
            break;
        }
    }
    auto searchEnd = std::chrono::steady_clock::now();
    long long duration = std::chrono::duration_cast<std::chrono::milliseconds>(searchEnd - gSearchStart).count();
    
    std::cout << "info time spent searching: " << duration << " ms" << std::endl;

    return bestMoveSoFar; // Return the best move found within time/depth limits 
}