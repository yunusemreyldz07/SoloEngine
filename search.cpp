#include "evaluation.h"
#include "board.h"
#include "search.h"
#include "bitboard.h"
#include <vector>
#include <algorithm>
#include <chrono>
#include <limits>
#include <iostream>
#include <atomic>

int MATE_SCORE = 100000;

// Global search tuning knobs. Updated via set_search_params for A/B tests.
// Next test profile: LMR off, others on.
static SearchParams gSearchParams{
    /*use_lmr=*/false,
    /*use_lmp=*/true,
    /*use_aspiration=*/true,
    /*use_qsearch_see=*/false,
    /*lmr_min_depth=*/3,
    /*lmr_min_moves=*/4,
    /*lmp_min_depth=*/4,
    /*lmp_max_depth=*/8,
    /*aspiration_delta=*/50
};

const SearchParams& get_search_params() {
    return gSearchParams;
}

void set_search_params(const SearchParams& params) {
    gSearchParams = params;
}

int historyTable[64][64];

const int PIECE_VALUES[7] = {0, 100, 320, 330, 500, 900, 20000};

Move killerMove[2][100];
std::atomic<long long> nodeCount{0};
void resetNodeCounter() {
    nodeCount.store(0, std::memory_order_relaxed);
}

long long getNodeCounter() {
    return nodeCount.load(std::memory_order_relaxed);
}

std::atomic<bool> stop_search(false);
std::atomic<long long> time_limit_ms{0};          // Time limit (atomic for thread-safety)
std::atomic<long long> start_time_ms{0};          // Start time in milliseconds since epoch (atomic for thread-safety)
std::atomic<bool> is_time_limited{false};         // Do we have time limit? (atomic for thread-safety)
std::atomic<bool> use_tt(true);

void request_stop_search() {
    stop_search.store(true, std::memory_order_relaxed);
}

void set_use_tt(bool enabled) {
    use_tt.store(enabled, std::memory_order_relaxed);
}

static bool is_square_attacked_otf(const Board& board, int row, int col, bool byWhite) {
    int sq = row_col_to_sq(row, col);
    const int us = byWhite ? WHITE : BLACK;
    Bitboard occ = board.color[WHITE] | board.color[BLACK];

    Bitboard pawns = board.piece[pawn - 1] & board.color[us];
    Bitboard knights = board.piece[knight - 1] & board.color[us];
    Bitboard bishops = board.piece[bishop - 1] & board.color[us];
    Bitboard rooks = board.piece[rook - 1] & board.color[us];
    Bitboard queens = board.piece[queen - 1] & board.color[us];
    Bitboard kings = board.piece[king - 1] & board.color[us];

    if (byWhite) {
        if (pawn_attacks[BLACK][sq] & pawns) return true;
    } else {
        if (pawn_attacks[WHITE][sq] & pawns) return true;
    }

    if (knight_attacks[sq] & knights) return true;
    if (king_attacks[sq] & kings) return true;

    if (bishop_attacks_otf(sq, occ) & (bishops | queens)) return true;
    if (rook_attacks_otf(sq, occ) & (rooks | queens)) return true;

    return false;
}


// Is the time limit reached?
bool should_stop() {
    if (stop_search.load(std::memory_order_relaxed)) return true;
    
    if (is_time_limited.load(std::memory_order_relaxed)) {
        // Checking the system clock every time is expensive, so we filter with nodeCount
        if ((nodeCount.load(std::memory_order_relaxed) & 2047) == 0) { // Check every 2048 nodes
            auto now = std::chrono::steady_clock::now();
            long long now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
            long long elapsed = now_ms - start_time_ms.load(std::memory_order_relaxed);
            if (elapsed >= time_limit_ms.load(std::memory_order_relaxed)) {
                stop_search.store(true, std::memory_order_relaxed);
                return true;
            }
        }
    }
    return false;
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
    int from = row_col_to_sq(move.fromRow, move.fromCol);
    int to = row_col_to_sq(move.toRow, move.toCol);
    if (move.capturedPiece != 0 || move.isEnPassant) {
        int victimPiece = move.isEnPassant ? pawn : std::abs(move.capturedPiece);
        int victimValue = PIECE_VALUES[victimPiece];

        int attackerPiece = piece_at_sq(board, from);
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

    // Root move ordering bias for sensible openings (only ply 0)
    if (ply == 0) {
        // Encourage central pawn pushes (d/e pawns two squares)
        if (move.fromRow == 6 && move.toRow == 4 && (move.fromCol == 3 || move.fromCol == 4)) {
            moveScore += 500;
        }
        // Encourage knights toward center (Nb1-c3, Ng1-f3 and mirrors for black)
        int piece = piece_at_sq(board, from);
        if (std::abs(piece) == knight) {
            // White: b1->c3, g1->f3; Black: b8->c6, g8->f6
            if ((move.fromRow == 7 && move.fromCol == 1 && move.toRow == 5 && move.toCol == 2) ||
                (move.fromRow == 7 && move.fromCol == 6 && move.toRow == 5 && move.toCol == 5) ||
                (move.fromRow == 0 && move.fromCol == 1 && move.toRow == 2 && move.toCol == 2) ||
                (move.fromRow == 0 && move.fromCol == 6 && move.toRow == 2 && move.toCol == 5)) {
                moveScore += 400;
            }
        }
    }

    if (move.isCastling) {
        // Castling is good for king safety, but keep the bonus modest so we don't prefer it over
        // urgent defensive moves (like saving a hanging piece) at shallow depth
        moveScore += 500;
    }

    if (historyTable[from][to] != 0) {
        moveScore += historyTable[from][to];
    }

    return moveScore;
}

int quiescence(Board& board, int alpha, int beta, int ply){
    nodeCount.fetch_add(1, std::memory_order_relaxed);
    if (should_stop()) {
        return 0; // Search was stopped
    }

    if (ply >= 99) {
        return evaluate_board(board); // Prevent infinite quiescence depth and overflows
    }

    // Do not stop until you reach a quiet position
    int stand_pat = evaluate_board(board);

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
        // Optional SEE-based pruning for quiescence (off by default for testing).
        if (gSearchParams.use_qsearch_see) {
            Move seeMove = move;
            if (see_exchange(board, seeMove) < -100) {
                continue;
            }
        }

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

    if (should_stop()) {
        return 0; // Search was stopped
    }

    const bool ttEnabled = true;
    bool pvNode = (beta - alpha) > 1;
    bool firstMove = true;
    const int alphaOrig = alpha;
    int maxEval = -200000;
    int MATE_VALUE = 100000;

    if (depth <= 0) {
        pvLine.clear();
        return quiescence(board, alpha, beta, ply);
    }

    if (is_threefold_repetition(history)) {
        pvLine.clear();
        return 0; // Draw
    }
    int kRow = board.isWhiteTurn ? board.whiteKingRow : board.blackKingRow;
    int kCol = board.isWhiteTurn ? board.whiteKingCol : board.blackKingCol;
    bool inCheck = is_square_attacked(board, kRow, kCol, !board.isWhiteTurn);
    uint64_t currentHash = position_key(board);
    int ttScore = 0;
    int ttDepth = 0;
    TTFlag ttFlag = TTFlag::EXACT;
    Move ttMove;
    bool ttHit = false;
    ttHit = globalTT.probe(currentHash, ttScore, ttDepth, ttFlag, ttMove);

    int movesSearched = 0;
    int eval = -MATE_VALUE;
    bool is_repetition_candidate = false;

    if (history.size() > 1) {
        for (int i = static_cast<int>(history.size()) - 2; i >= 0; i--) {
            if (history[i] == currentHash) {
                is_repetition_candidate = true;
                break;
            }
        }
    }

    // Static evaluation (from side-to-move perspective). Used by forward/reverse pruning.
    const int staticEval = evaluate_board(board);
    if (!is_repetition_candidate && ttHit && ttDepth >= depth) {
        if (ttFlag == EXACT) {
            pvLine.clear();
            return ttScore;
        }

        if (!is_repetition_candidate && ttFlag == ALPHA && ttScore <= alpha) {
            pvLine.clear();
            return alpha;
        }
        if (!is_repetition_candidate && ttFlag == BETA && ttScore >= beta) {
            pvLine.clear();
            return beta;
        }
    }


    // Reverse Futility Pruning 
    // Only makes sense in non-PV nodes (null-window), otherwise it can prune good PV continuations.
    if ((beta - alpha) == 1 && depth < 9 && !inCheck && beta < MATE_SCORE - 100) {
        
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

        if (!inCheck && depth >= 3 && (beta - alpha == 1)) {
            // Make a "null move" by flipping side to move
            const int prevEnPassantCol = board.enPassantCol;
            board.enPassantCol = -1; // En passant rights vanish after a null move.
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
            board.enPassantCol = prevEnPassantCol;

            if (nullScore >= beta) {
                pvLine.clear();
                return beta; // Null-move cutoff
            }
        }
    }

    std::vector<Move> possibleMoves = get_all_moves(board, board.isWhiteTurn);
    Move bestMove = possibleMoves.empty() ? Move() : possibleMoves[0];

    if (possibleMoves.empty()) {
        int kingRow = board.isWhiteTurn ? board.whiteKingRow : board.blackKingRow;
        int kingCol = board.isWhiteTurn ? board.whiteKingCol : board.blackKingCol;
        if (is_square_attacked(board, kingRow, kingCol, !board.isWhiteTurn)) 
            return -MATE_VALUE + ply; // Mate
        return 0; // Stalemate
    }

    // Move Ordering
    std::sort(possibleMoves.begin(), possibleMoves.end(), [&](const Move& a, const Move& b) {
        const Move* ttMovePtr = ttHit ? &ttMove : nullptr;
        return scoreMove(board, a, ply, ttMovePtr) > scoreMove(board, b, ply, ttMovePtr);
    });
    
    for (Move& move : possibleMoves) {
        int lmpCount = (3 * depth * depth) + 4;
        if (gSearchParams.use_lmp && !pvNode && depth >= gSearchParams.lmp_min_depth && depth <= gSearchParams.lmp_max_depth && movesSearched >= lmpCount && !inCheck && move.promotion == 0 && move.capturedPiece == 0) {
            if (!move.isEnPassant) {
                bool isKiller = false;
                if (ply < 100) {
                    if (move.fromCol == killerMove[0][ply].fromCol && move.fromRow == killerMove[0][ply].fromRow &&
                        move.toCol == killerMove[0][ply].toCol && move.toRow == killerMove[0][ply].toRow) isKiller = true;
                    else if (move.fromCol == killerMove[1][ply].fromCol && move.fromRow == killerMove[1][ply].fromRow &&
                        move.toCol == killerMove[1][ply].toCol && move.toRow == killerMove[1][ply].toRow) isKiller = true;
                }
                
                if (!isKiller) {
                    continue;
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
            if (gSearchParams.use_lmr && move.capturedPiece == 0 && !move.isEnPassant && move.promotion == 0 && depth >= gSearchParams.lmr_min_depth && movesSearched > gSearchParams.lmr_min_moves) {
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
            int from = row_col_to_sq(move.fromRow, move.fromCol);
            int to = row_col_to_sq(move.toRow, move.toCol);
            
            if (from >= 0 && from < 64 && to >= 0 && to < 64) {
                 historyTable[from][to] += depth * depth;
            }
            
            break; // beta cutoff
        }
    }

    TTFlag flag;
    if (maxEval <= alphaOrig) flag = ALPHA;
    else if (maxEval >= beta) flag = BETA;
    else flag = EXACT;
    
    globalTT.store(currentHash, maxEval, depth, flag, bestMove);
    return maxEval;
}

Move getBestMove(Board& board, int maxDepth, int movetimeMs, const std::vector<uint64_t>& history, int ply) {

    // Reset variables
    resetNodeCounter();
    stop_search.store(false, std::memory_order_relaxed);
    
    // Time settings
    auto now = std::chrono::steady_clock::now();
    start_time_ms.store(std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count(), std::memory_order_relaxed);
    if (movetimeMs > 0) {
        is_time_limited.store(true, std::memory_order_relaxed);
        long long time_limit = movetimeMs; // Do not use the entire time given by the GUI leave a small margin
        if (time_limit > 50) time_limit -= 20; // 20ms safety margin
        time_limit_ms.store(time_limit, std::memory_order_relaxed);
    } else {
        is_time_limited.store(false, std::memory_order_relaxed);
        time_limit_ms.store(0, std::memory_order_relaxed);
    }

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

    std::vector<uint64_t> localHistory;
    localHistory.reserve(std::min((size_t)100, history.size()) + 1);
    auto startIt = (history.size() > 100) ? (history.end() - 100) : history.begin(); // Keep only last 100 entries
    localHistory.assign(startIt, history.end());

    int lastScore = 0; // for aspiration windows

    for (int depth = 1; depth <= effectiveMaxDepth; depth++) {

        if (stop_search.load(std::memory_order_relaxed)) break;

        const bool useAsp = gSearchParams.use_aspiration;
        const int INF = 2000000000;
        int delta = gSearchParams.aspiration_delta; // Aspiration window half-width
        int alpha = -INF;
        int beta = INF;

        if (useAsp && depth > 1) {
            alpha = std::max(-INF, lastScore - delta);
            beta = std::min(INF, lastScore + delta);
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
                std::stable_sort(possibleMoves.begin(), possibleMoves.end(), [&](const Move& a, const Move& b) {
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

            std::vector<Move> bestPvForDepth;
            bestPvForDepth.reserve(depth + 4);
            for (Move move : possibleMoves) {
                if (stop_search.load(std::memory_order_relaxed)) {
                    thisDepthCompleted = false;
                    break; 
                }

                board.makeMove(move);

                std::vector<Move> childPv;

                uint64_t newHash = position_key(board);
                localHistory.push_back(newHash);
                int val = -negamax(board, depth - 1, -beta, -alpha, ply + 1, localHistory, childPv);
                localHistory.pop_back();
                
                board.unmakeMove(move);

                if (stop_search.load(std::memory_order_relaxed)) {
                    thisDepthCompleted = false;
                    break; 
                }

                if (val > bestValue) {
                    bestValue = val;
                    currentDepthBestMove = move;
                    bestPvForDepth.clear();
                    bestPvForDepth.push_back(move);
                    bestPvForDepth.insert(bestPvForDepth.end(), childPv.begin(), childPv.end());
                    
                    if (bestValue > alpha) {
                        alpha = bestValue;

                        if (alpha >= beta) {
                            break;
                        }
                    }
                }
            }

            if (stop_search.load(std::memory_order_relaxed)) {
                break; 
            }

            // Aspiration window re-search logic
            if (useAsp && depth >= 5 && (bestValue <= alphaStart || bestValue >= betaStart)) {
                // Fail-low or fail-high: widen the window and re-search this depth.
                alpha = std::max(-INF, bestValue - delta);
                beta = std::min(INF, bestValue + delta);
                delta += delta / 2;
                continue; // Restart the depth search
            }


            if (thisDepthCompleted && (bestValue > std::numeric_limits<int>::min() / 2)) {
                bestMoveSoFar = currentDepthBestMove;
                lastScore = bestValue;
                auto searchEnd = std::chrono::steady_clock::now();
                long long duration = std::chrono::duration_cast<std::chrono::milliseconds>(searchEnd - gSearchStart).count();
                std::cout << "info depth " << depth << " ";
                long long nps = 0;
                if (duration > 0) {
                    nps = (getNodeCounter() * 1000LL) / duration;
                }
                std::cout << "score cp " << bestValue;
                std::cout << " time " << duration
                            << " nps " << nps
                            << " pv ";
                for (const Move& pvMove : bestPvForDepth) {
                    std::cout << move_to_uci(pvMove) << " ";
                }
                std::cout << std::endl;
            }
            break; // Exit aspiration window loop
        }
        
        if (stop_search.load(std::memory_order_relaxed) || bestValue >= 100000 - 50) {
            break;
        }
    }

    return bestMoveSoFar; // Return the best move found within time/depth limits 
}
