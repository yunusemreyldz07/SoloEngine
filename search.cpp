#include "evaluation.h"
#include "board.h"
#include "search.h"
#include <vector>
#include <algorithm>
#include <chrono>
#include <limits>
#include <iostream>
#include <atomic>

int MATE_SCORE = 100000;

int historyTable[64][64];

Move killerMove[2][100];
std::atomic<long long> nodeCount{0};
void resetNodeCounter() {
    nodeCount.store(0, std::memory_order_relaxed);
}

long long getNodeCounter() {
    return nodeCount.load(std::memory_order_relaxed);
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

int scoreMove(const Board& board, const Move& move, int ply, const Move* ttMove) {
    int moveScore = 0;
    int from = move.fromRow * 8 + move.fromCol;
    int to = move.toRow * 8 + move.toCol;
    if (move.capturedPiece != 0 || move.isEnPassant) {
        int victimPiece = move.isEnPassant ? pawn : std::abs(move.capturedPiece);
        int victimValue = PIECE_VALUES[victimPiece];

        int attackerPiece = board.squares[move.fromRow][move.fromCol];
        int attackerValue = PIECE_VALUES[std::abs(attackerPiece)];
        moveScore += 10000 + (victimValue * 10) - attackerValue;
    }

    if (ttMove != nullptr && 
        move.fromRow == ttMove->fromRow && move.fromCol == ttMove->fromCol &&
        move.toRow == ttMove->toRow && move.toCol == ttMove->toCol) {
        return 1000000; // TT move always has the highest priority
    }

    if (ply >= 0 && ply < 100) { 
        if (move.fromCol == killerMove[0][ply].fromCol && move.fromRow == killerMove[0][ply].fromRow &&
            move.toCol == killerMove[0][ply].toCol && move.toRow == killerMove[0][ply].toRow) {
            moveScore += 8000;
        }

        if (move.fromCol == killerMove[1][ply].fromCol && move.fromRow == killerMove[1][ply].fromRow &&
            move.toCol == killerMove[1][ply].toCol && move.toRow == killerMove[1][ply].toRow) {
            moveScore += 7000;
        }
    }

    if (move.promotion != 0) {
        moveScore += 9000;
    }

    if (move.isCastling) {
        // Castling is good for king safety, but keep the bonus modest so we don't prefer it over
        // urgent defensive moves (like saving a hanging piece) at shallow depth.
        moveScore += 500;
    }

    if (historyTable[from][to] != 0) {
        moveScore += historyTable[from][to];
    }

    return moveScore;
}

int quiescence(Board& board, int alpha, int beta, int ply){
    // Do not stop until you reach a quiet position
    int stand_pat = board.isWhiteTurn ? evaluate_board(board) : -evaluate_board(board);

    // Alpha-Beta pruning
    if (stand_pat >= beta) {
        return beta;
    }
    if (alpha < stand_pat) {
        alpha = stand_pat;
    }

    std::vector<Move> captureMoves = get_capture_moves(board);

    std::sort(captureMoves.begin(), captureMoves.end(), [&](const Move& a, const Move& b) {
        return scoreMove(board, a, ply, nullptr) > scoreMove(board, b, ply, nullptr);
    });

    for (Move& move : captureMoves) {
        // Delta Pruning
        // If even the most optimistic evaluation (stand_pat + value of captured piece + margin) is worse than alpha, skip 
        int capturedValue = PIECE_VALUES[std::abs(move.capturedPiece)];
        if (stand_pat + capturedValue + 200 < alpha) {
            continue; 
        }

        board.makeMove(move);

        int kingRow = board.isWhiteTurn ? board.blackKingRow : board.whiteKingRow;
        int kingCol = board.isWhiteTurn ? board.blackKingCol : board.whiteKingCol;
        
        if (is_square_attacked(board, kingRow, kingCol, board.isWhiteTurn)) {
            board.unmakeMove(move);
            continue; // illegal move
        }
        int eval = -quiescence(board, -beta, -alpha, ply + 1);
        board.unmakeMove(move);

        if (eval >= beta) {
            return beta;
        }
        if (eval > alpha) {
            alpha = eval;
        }
    }

    return alpha;
}

// Negamax
int negamax(Board& board, int depth, int alpha, int beta, int ply, std::vector<uint64_t>& history, std::vector<Move>& pvLine) {
    nodeCount.fetch_add(1, std::memory_order_relaxed);
    bool pvNode = (beta - alpha) > 1;
    bool firstMove = true;
    const int alphaOrig = alpha;
    int maxEval = -200000;
    int MATE_VALUE = 100000;

    if (depth == 0) {
        pvLine.clear();
        return quiescence(board, alpha, beta, ply);
    }

    if (is_threefold_repetition(history)) {
        pvLine.clear();
        return 0; // Draw
    }
    bool inCheck = is_square_attacked(board, board.isWhiteTurn ? board.whiteKingRow : board.blackKingRow, board.isWhiteTurn ? board.whiteKingCol : board.blackKingCol, !board.isWhiteTurn);
    uint64_t currentHash = position_key(board);
    int ttScore = 0;
    int ttDepth = 0;
    TTFlag ttFlag = TTFlag::EXACT;
    Move ttMove;
    const bool ttHit = globalTT.probe(currentHash, ttScore, ttDepth, ttFlag, ttMove);

    auto history_pop = [&history]() { if (!history.empty()) history.pop_back(); };

    int movesSearched = 0;
    int eval = -MATE_VALUE;

    // Static evaluation (from side-to-move perspective). Used by forward/reverse pruning.
    const int staticEval = board.isWhiteTurn ? evaluate_board(board) : -evaluate_board(board);
    if (ttHit && ttDepth >= depth) {
        if (ttFlag == EXACT) {
            pvLine.clear();
            return ttScore;
        }
        if (ttFlag == ALPHA && ttScore <= alpha) {
            pvLine.clear();
            return alpha;
        }
        if (ttFlag == BETA && ttScore >= beta) {
            pvLine.clear();
            return beta;
        }
    }


    // Reverse Futility Pruning 
    // Only makes sense in non-PV nodes (null-window), otherwise it can prune good PV continuations.
    if ((beta - alpha) == 1 && depth < 9 && !inCheck && !is_endgame(board) && beta < MATE_SCORE - 100) {
        
        // margin: for every depth, we allow a margin of 100 centipawns
        // The deeper we go, the larger the margin should be
        int margin = 80 * depth; 

        if (staticEval - margin >= beta) {
            // "I'm so far ahead that even if I reduce the margin, I still surpass the opponent's threshold, so I don't need to search further and lose time"
            pvLine.clear();
            return beta; // Cutoff
        }
    }

    // Null move pruning
    {
        int kingRow = board.isWhiteTurn ? board.whiteKingRow : board.blackKingRow;
        int kingCol = board.isWhiteTurn ? board.whiteKingCol : board.blackKingCol;
        // Check if side to move is currently in check
        bool inCheck = is_square_attacked(board, kingRow, kingCol, !board.isWhiteTurn);

        if (!inCheck && depth >= 3 && (beta - alpha == 1) && !is_endgame(board)) {
            // Make a "null move" by flipping side to move
            board.isWhiteTurn = !board.isWhiteTurn;

            // Push new position key to history so threefold repetition checks remain correct
            uint64_t nullHash = position_key(board);
            history.push_back(nullHash);

            // Reduction factor R (typical values 2..3). Ensure we don't search negative depth
            int R = std::min(3, std::max(1, depth - 2));
            std::vector<Move> nullPv;
            int nullScore = -negamax(board, depth - 1 - R, -beta, -beta + 1, ply + 1, history, nullPv);

            // Undo history change and null move
            if (!history.empty()) history.pop_back();
            board.isWhiteTurn = !board.isWhiteTurn;

            if (nullScore >= beta) {
                pvLine.clear();
                return beta; // Null-move cutoff
            }
        }
    }

    std::vector<Move> possibleMoves = get_all_moves(board, board.isWhiteTurn);
    Move bestMove = possibleMoves.empty() ? Move() : possibleMoves[0];

    if (possibleMoves.empty()) {
        history_pop();
        if (is_square_attacked(board, board.isWhiteTurn ? board.whiteKingRow : board.blackKingRow, 
                               board.isWhiteTurn ? board.whiteKingCol : board.blackKingCol, !board.isWhiteTurn)) 
            return -MATE_VALUE + ply; // Mate
        return 0; // Stalemate
    }

    // Move Ordering
    std::sort(possibleMoves.begin(), possibleMoves.end(), [&](const Move& a, const Move& b) {
        const Move* ttMovePtr = ttHit ? &ttMove : nullptr;
        return scoreMove(board, a, ply, ttMovePtr) > scoreMove(board, b, ply, ttMovePtr);
    });
    
    for (Move& move : possibleMoves) {
        int lmpCount = (depth * depth) + 5;
        // Late Move Pruning (LMP) logic
        if (!pvNode && depth < 16 && movesSearched >= lmpCount && !inCheck) { // if the move is late enough in the move list (this is kinda like gambling, but we trust our move ordering) and the move is not a check
            if (move.capturedPiece == 0 && move.promotion == 0 && !move.isEnPassant) {
                bool isKiller = false;
                // Checking if the move is a killer move, they are important so we should not prune them
                if (ply < 100) {
                    if (move.fromCol == killerMove[0][ply].fromCol && move.fromRow == killerMove[0][ply].fromRow &&
                        move.toCol == killerMove[0][ply].toCol && move.toRow == killerMove[0][ply].toRow) isKiller = true;
                    else if (move.fromCol == killerMove[1][ply].fromCol && move.fromRow == killerMove[1][ply].fromRow &&
                        move.toCol == killerMove[1][ply].toCol && move.toRow == killerMove[1][ply].toRow) isKiller = true;
                }
                
                if (!isKiller) {
                    continue; // skip this move (late move pruning)
                }
            }
        }

        board.makeMove(move);
        movesSearched++;
        std::vector<Move> childPv;
        uint64_t newHash = position_key(board);
        history.push_back(newHash);
        if (firstMove){
            eval = -negamax(board, depth - 1, -beta, -alpha, ply + 1, history, childPv);
            firstMove = false;
        }
        else {
            // Late Move Reduction (LMR)
            int reduction = 0;
            std::vector<Move> nullWindowPv;
            if (move.capturedPiece == 0 && !move.isEnPassant && move.promotion == 0 && depth >= 3 && movesSearched > 4) {
                reduction = 1 + (depth / 6); // Increase reduction with depth

                if (depth - 1 - reduction < 1) reduction = depth - 2; // Ensure we don't search negative depth
            }
            
            eval = -negamax(board, depth - 1 - reduction, -alpha - 1, -alpha, ply + 1, history, nullWindowPv);

            if (reduction > 0 && eval > alpha) {
                // Re-search at full depth if reduced search suggests a better move
                eval = -negamax(board, depth - 1, -alpha - 1, -alpha, ply + 1, history, childPv);
            }

            if (eval > alpha && eval < beta) {
                childPv.clear();
                eval = -negamax(board, depth - 1, -beta, -alpha, ply + 1, history, childPv);
            } else {
                childPv.clear();
            }
        }
        if (!history.empty()) history.pop_back();
        board.unmakeMove(move);

        if (eval > maxEval) {
            maxEval = eval;
            bestMove = move;
            pvLine.clear();
            pvLine.push_back(move);
            pvLine.insert(pvLine.end(), childPv.begin(), childPv.end());
        }

        if (eval > alpha) {
            alpha = eval;
        }

        if (beta <= alpha) {
            if (move.capturedPiece == 0 && !move.isEnPassant && move.promotion == 0) {
                 if (ply < 100 && !(move.fromCol == killerMove[0][ply].fromCol && move.fromRow == killerMove[0][ply].fromRow &&
                      move.toCol == killerMove[0][ply].toCol && move.toRow == killerMove[0][ply].toRow)){
                    killerMove[1][ply] = killerMove[0][ply];
                    killerMove[0][ply] = move;
                }
            }
            int from = move.fromRow * 8 + move.fromCol;
            int to = move.toRow * 8 + move.toCol;
            
            if (from >= 0 && from < 64 && to >= 0 && to < 64) {
                 historyTable[from][to] += depth * depth;
            }
            
            break; // beta cutoff
        }
    }

    history_pop();
    
    TTFlag flag;
    if (maxEval <= alphaOrig) flag = ALPHA;
    else if (maxEval >= beta) flag = BETA;
    else flag = EXACT;
    
    globalTT.store(currentHash, maxEval, depth, flag, bestMove);
    return maxEval;
}

Move getBestMove(Board& board, int maxDepth, int movetimeMs, const std::vector<uint64_t>& history, int ply) {
    bool isWhite = board.isWhiteTurn;
    std::vector<Move> possibleMoves = get_all_moves(board, isWhite);
    std::sort(possibleMoves.begin(), possibleMoves.end(), [&](const Move& a, const Move& b) {
        return scoreMove(board, a, 0, nullptr) > scoreMove(board, b, 0, nullptr);
    });
    if (possibleMoves.empty()) return {};
    if (possibleMoves.size() == 1) return possibleMoves[0];

    Move bestMoveSoFar = possibleMoves[0]; 

    auto gSearchStart = std::chrono::steady_clock::now();
    auto gTimeLimited = (movetimeMs > 0);
    auto gTimeLimitMs = gTimeLimited ? movetimeMs : std::numeric_limits<int>::max();
    const int effectiveMaxDepth = gTimeLimited ? 128 : maxDepth;
    int bestValue;
    bool timeExpired = false;

    int lastScore = 0; // for aspiration windows

    for (int depth = 1; depth <= effectiveMaxDepth; depth++) {
        
        if (gTimeLimited) {
            auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::milliseconds>(now - gSearchStart).count() >= gTimeLimitMs) {
                break;
            }
        }

        int delta = 50; // Aspiration window margin
        int alpha = -2000000000;
        int beta = 2000000000;
        
        if (depth >= 5){
            alpha = std::max(-2000000000, lastScore - delta);
            beta = std::min(2000000000, lastScore + delta);
        }
        while (true) {
            const int alphaStart = alpha;
            const int betaStart = beta;
            bestValue = std::numeric_limits<int>::min() / 2;
            Move currentDepthBestMove; // Only the best move found at this depth
            bool thisDepthCompleted = true; // Is this depth completed?

            // PV Move Ordering (Put the best move from the previous depth first)
            if (depth > 1) {
                // Here we use bestMoveSoFar because it was the winner of the previous depth
                Move pvMove = bestMoveSoFar; 
                std::sort(possibleMoves.begin(), possibleMoves.end(), [&](const Move& a, const Move& b) {
                    const bool aIsPV = (a.fromRow == pvMove.fromRow && a.fromCol == pvMove.fromCol && a.toRow == pvMove.toRow && a.toCol == pvMove.toCol && a.promotion == pvMove.promotion);
                    const bool bIsPV = (b.fromRow == pvMove.fromRow && b.fromCol == pvMove.fromCol && b.toRow == pvMove.toRow && b.toCol == pvMove.toCol && b.promotion == pvMove.promotion);

                    // Strict-weak-ordering: comparator must be irreflexive (never a < a).
                    if (aIsPV != bIsPV) return aIsPV;

                    const int sa = scoreMove(board, a, 0, nullptr);
                    const int sb = scoreMove(board, b, 0, nullptr);
                    if (sa != sb) return sa > sb;

                    // Deterministic tie-breaker.
                    if (a.fromRow != b.fromRow) return a.fromRow < b.fromRow;
                    if (a.fromCol != b.fromCol) return a.fromCol < b.fromCol;
                    if (a.toRow != b.toRow) return a.toRow < b.toRow;
                    if (a.toCol != b.toCol) return a.toCol < b.toCol;
                    return a.promotion < b.promotion;
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

                std::vector<Move> childPv;
                std::vector<uint64_t> localHistory = history; // make a mutable copy for search
                uint64_t newHash = position_key(board);
                localHistory.push_back(newHash);
                int val = -negamax(board, depth - 1, -beta, -alpha, ply + 1, localHistory, childPv);
                
                board.unmakeMove(move);

                if (val > bestValue) {
                    bestValue = val;
                    currentDepthBestMove = move;
                    
                    if (bestValue > alpha) {
                        alpha = bestValue;

                        if (alpha >= beta) {
                            // Beta cutoff occurred, this shouldnt have happened in aspiration window so we need to re-search
                            break;
                        }
                    }
                    
                    if (thisDepthCompleted) { 
                        lastScore = bestValue;
                        bestMoveSoFar = currentDepthBestMove;
                        auto searchEnd = std::chrono::steady_clock::now();
                        long long duration = std::chrono::duration_cast<std::chrono::milliseconds>(searchEnd - gSearchStart).count();
                        std::cout << "info depth " << depth 
                                    << " score cp " << bestValue 
                                    << " time " << duration 
                                    << " pv " << move_to_uci(bestMoveSoFar) << std::endl;
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
            // Aspiration window re-search logic
            if (depth >= 5 && (bestValue <= alphaStart || bestValue >= betaStart)) {
                // Fail-low or fail-high: widen the window and re-search this depth.
                alpha = std::max(-2000000000, bestValue - delta);
                beta = std::min(2000000000, bestValue + delta);
                delta += delta / 2;
                continue; // Restart the depth search
            }


            if (thisDepthCompleted && (bestValue > std::numeric_limits<int>::min() / 2)) {
                bestMoveSoFar = currentDepthBestMove;
            }
            break; // Exit aspiration window loop
        }
        

        if (bestValue >= MATE_SCORE - 50) {
            break;
        }
    }

    return bestMoveSoFar; // Return the best move found within time/depth limits 
}
