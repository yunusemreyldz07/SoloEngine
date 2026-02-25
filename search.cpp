#include "search.h"
#include "uci.h"
#include "bitboard.h"
#include "evaluation.h"
#include "board.h"

#include <atomic>
#include <iostream>
#include <vector>
#include <chrono>
#include <algorithm>

#define MAX_MOVES 256
#define SEE_THRESHOLD -82

namespace {
std::atomic<bool> stop_search{false};
std::atomic<long long> nodeCount{0};
std::atomic<long long> start_time_ms{0};
std::atomic<long long> time_limit_ms{0};
std::atomic<bool> time_limited{false};

const int PIECE_VALUES[7] = {0, 100, 320, 330, 500, 900, 20000};

inline long long now_ms() {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
}

inline bool should_stop_search() {
    if (stop_search.load(std::memory_order_relaxed)) return true;
    if (!time_limited.load(std::memory_order_relaxed)) return false;
    if ((nodeCount.load(std::memory_order_relaxed) & 2047) == 0) {
        long long elapsed = now_ms() - start_time_ms.load(std::memory_order_relaxed);
        if (elapsed >= time_limit_ms.load(std::memory_order_relaxed)) {
            stop_search.store(true, std::memory_order_relaxed);
            return true;
        }
    }
    return false;
}
}

void initLMRtables() {
    // No-op for now.
}

void resetNodeCounter() {
    nodeCount.store(0, std::memory_order_relaxed);
}

long long getNodeCounter() {
    return nodeCount.load(std::memory_order_relaxed);
}

int scoreMove(Board& board, const Move& move, Move ttMove = 0) {
    int score = 0;
    int from = move_from(move);
    int to = move_to(move);
    int piece = piece_at_sq(board, from);
    int flags = move_flags(move);

    if (is_capture(move)) {
        int victimPiece = flags == FLAG_EN_PASSANT ? PAWN : piece_type(board.mailbox[to]);
        int victimValue = PIECE_VALUES[victimPiece];

        int attackerPiece = piece_at_sq(board, from);
        int attackerValue = PIECE_VALUES[piece_type(attackerPiece)];
        int mvvScore = victimValue * 10 - attackerValue;
        score += mvvScore;
    }

    if (ttMove != 0 && move == ttMove) {
        score += SCORE_TT_MOVE;
    }
    return score;
}

void orderMoves(Board& board, Move* moves, int moveCount, Move ttMove = 0) {
    std::sort(moves, moves + moveCount, [&](const Move& a, const Move& b) {
        return scoreMove(board, a, ttMove) > scoreMove(board, b, ttMove);
    });
}

int16_t qsearch(Board& board, int16_t alpha, int16_t beta, int ply) {
    if (stop_search.load(std::memory_order_relaxed)) return 0;
    nodeCount.fetch_add(1, std::memory_order_relaxed);
    int stand_pat = evaluate_board(board);

    if (stand_pat >= beta) {
        return stand_pat;
    }

    if (stand_pat > alpha) {
        alpha = stand_pat;
    }

    Move captureMoves[MAX_MOVES];
    int moveCount = 0;
    get_capture_moves(board, captureMoves, moveCount);
    orderMoves(board, captureMoves, moveCount, 0);
    int bestEval = stand_pat;

    for (int i = 0; i < moveCount; ++i) {
        Move captureMove = captureMoves[i];
        
        board.makeMove(captureMove);
        
        int eval = -qsearch(board, -beta, -alpha, ply + 1);
        
        board.unmakeMove(captureMove);

        if (eval > bestEval) {
            bestEval = eval;
        }

        if (eval > alpha) {
            alpha = eval;
        }

        if (alpha >= beta) {
            break; // Beta cutoff
        }
    }

    return bestEval;
}

int16_t negamax(Board& board, int depth, int16_t alpha, int16_t beta, int ply, std::vector<Move>& pvLine) {
    nodeCount.fetch_add(1, std::memory_order_relaxed);
    if (should_stop_search()) return 0;

    if (depth <= 0) {
        return qsearch(board, alpha, beta, ply);
    }

    bool firstMove = true; // for PVS
    int16_t eval = 0; 

    const int16_t staticEval = evaluate_board(board);
    const bool pvNode = (beta - alpha > 1);

    int kingSq = 0;
    king_square(board, board.stm == WHITE, kingSq);
    bool inCheck = is_square_attacked(board, kingSq, board.stm != WHITE);

    int16_t originalAlpha = alpha;
    uint64_t hashKey = board.hash; 
    TTEntry& ttEntry = ttTable.getEntry(hashKey);
    Move ttMove = 0;
    bool ttHit = false;
    if (ttEntry.hashKey == hashKey) {
        ttMove = ttEntry.bestMove;
        ttHit = true;
        if (ttEntry.depth >= depth && ply > 0) {
            int16_t ttScore = ttEntry.score;
            if (ttEntry.flag == TT_EXACT) {
                return ttScore;
            }
            if (ttEntry.flag == TT_BETA && ttScore <= alpha) {
                return ttScore;
            }
            if (ttEntry.flag == TT_ALPHA && ttScore >= beta) {
                return ttScore;
            }
        
        }
    }

    int moveCount = 0;
    Move moves[MAX_MOVES];
    get_all_moves(board, moves, moveCount);

    if (moveCount == 0) {
        int kingSq = -1;
        king_square(board, board.stm == WHITE, kingSq);
        if ((kingSq != -1) && is_square_attacked(board, kingSq, board.stm != WHITE)) {
            return -MATE_SCORE + ply;
        }
        return 0; // Stalemate
    }

    int16_t bestEval = -VALUE_INF;
    bool aborted = false;
    
    orderMoves(board, moves, moveCount, ttMove);
    Move bestMove = 0;

    // Reverse Futility Pruning
    if (!pvNode && depth < 9 && beta < MATE_SCORE - 100){

        int margin = 80 * depth;

        if (staticEval - margin >= beta) {
            // I am so far ahead that even if I reduce my score by the margin, I am still above beta. No need to search this node.
            return staticEval - margin;
        }
    }

    // Null Move Pruning
    if (!inCheck && depth >= 3 && !pvNode) {
        const int prevEnPassant = board.enPassant;
        const uint64_t prevHash = board.hash;

        board.enPassant = -1; // Clear EP square to prevent illegal null move
        board.stm = other_color(board.stm); // Switch side to move

        board.hash ^= zobrist().side; // Update hash for side to move change

        int oldEp = (prevEnPassant != -1) ? (prevEnPassant % 8) : 8;
        board.hash ^= zobrist().epFile[oldEp];
        board.hash ^= zobrist().epFile[8];

        int R = std::min(3, std::max(1, depth - 2));
        std::vector<Move> nullPv;
        int16_t nullScore = -negamax(board, depth - R, -beta, -beta + 1, ply + 1, nullPv);
        
        board.stm = other_color(board.stm);
        board.enPassant = prevEnPassant;
        board.hash = prevHash;

        if (nullScore >= beta) {
            pvLine.clear();
            return beta; // Null-move cutoff
        }
    }

    for (int i = 0; i < moveCount; ++i) {
        if (should_stop_search()) {
            aborted = true;
            break;
        }

        Move chosenMove = moves[i];
        
        std::vector<Move> childPv; 
        
        board.makeMove(chosenMove);
        if (firstMove){
            eval = -negamax(board, depth - 1, -beta, -alpha, ply + 1, childPv);
            firstMove = false;
        } else {
            eval = -negamax(board, depth - 1, -alpha - 1, -alpha, ply + 1, childPv); // PVS null window search
            if (eval > alpha && eval < beta) {
                childPv.clear();
                eval = -negamax(board, depth - 1, -beta, -alpha, ply + 1, childPv); // Re-search if we failed high
            }
        }
        board.unmakeMove(chosenMove);
        if (should_stop_search()) {
            aborted = true;
            break;
        }

        // fail soft
        if (eval > bestEval) {
            bestEval = eval;
        }

        if (eval > alpha) { 
            alpha = eval;
            bestMove = chosenMove;
            pvLine.clear();
            pvLine.push_back(chosenMove);
            pvLine.insert(pvLine.end(), childPv.begin(), childPv.end());
        }

        if (alpha >= beta) {
            break; // Beta cutoff
        }
    }

    if (aborted) {
        return bestEval; // Don't write to TT if search was aborted
    }

    TTFlag flag = TT_EXACT;
    if (alpha <= originalAlpha) {
        flag = TT_BETA;
    } else if (alpha >= beta) {
        flag = TT_ALPHA;
    }

    ttTable.writeEntry(hashKey, bestEval, static_cast<int8_t>(depth), flag, bestMove);


    return bestEval;
}

Move getBestMove(Board& board, int maxDepth, int movetimeMs, const std::vector<uint64_t>& positionHistory, int ply) {
    (void)positionHistory;
    int16_t score = 0;
    stop_search.store(false, std::memory_order_relaxed);
    if (movetimeMs > 0) {
        int safeTime = movetimeMs;
        if (safeTime > 50) safeTime -= 20;
        start_time_ms.store(now_ms(), std::memory_order_relaxed);
        time_limit_ms.store(safeTime, std::memory_order_relaxed);
        time_limited.store(true, std::memory_order_relaxed);
    } else {
        time_limited.store(false, std::memory_order_relaxed);
        time_limit_ms.store(0, std::memory_order_relaxed);
    }

    int moveCount = 0;
    Move moves[MAX_MOVES];
    get_all_moves(board, moves, moveCount);
    if (moveCount == 0) return 0;
    Move bestMoveSoFar = moves[0];

    // Iterative Deepening
    for(int iterativeDepth = 1; iterativeDepth <= maxDepth; ++iterativeDepth) {
        if (should_stop_search()) break;
        
        int16_t alpha = -VALUE_INF;
        int16_t beta = VALUE_INF;
        int delta = 9;
        const bool useAspiration = (iterativeDepth >= 4);
        std::vector<Move> pvLine;
        if (useAspiration) {
            alpha = std::max<int16_t>(alpha, static_cast<int16_t>(score - delta));
            beta = std::min<int16_t>(beta, static_cast<int16_t>(score + delta));
        }

        while (true) {
            pvLine.clear();
            int16_t searchScore = negamax(board, iterativeDepth, alpha, beta, ply, pvLine);

            if (should_stop_search()) {
                score = searchScore;
                break;
            }

            if (!useAspiration) {
                score = searchScore;
                break;
            }

            if (searchScore <= alpha) {
                delta = std::min<uint16_t>(4096, delta + (delta * 1.8));
                alpha = std::max<int16_t>(-VALUE_INF, static_cast<int16_t>(searchScore - delta));
                continue;
            }

            if (searchScore >= beta) {
                delta = std::min<uint16_t>(4096, delta + (delta * 1.8));
                beta = std::min<int16_t>(VALUE_INF, static_cast<int16_t>(searchScore + delta));
                continue;
            }

            score = searchScore;
            break;
        }

        if (should_stop_search()) {
            break;
        }

        if (!pvLine.empty()) {
            bestMoveSoFar = pvLine[0];
        }

        std::cout << "info depth " << iterativeDepth << " score cp " << score << " pv ";
        for (Move m : pvLine) {
            std::cout << move_to_uci(m) << " ";
        }
        std::cout << std::endl;
    }

    return bestMoveSoFar;
}
