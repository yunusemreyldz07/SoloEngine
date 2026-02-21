#include "evaluation.h"
#include "board.h"
#include "search.h"
#include "bitboard.h"
#include "history.h"
#include <vector>
#include <algorithm>
#include <chrono>
#include <limits>
#include <iostream>
#include <atomic>
#include <cstring>
#include <cmath>

const int PIECE_VALUES[7] = {0, 100, 320, 330, 500, 900, 20000};

std::atomic<long long> nodeCount{0};

int LMR_TABLE[256][256];
float LMR_BASE = 0.77f;
float LMR_DIVISION = 2.32f;

void initLMRtables(){
    for(int depth = 0; depth < 256; depth++){
        for(int moveNum = 0; moveNum < 256; moveNum++){
            if(depth < 2 || moveNum < 4 || depth == 0 || moveNum == 0){
                LMR_TABLE[depth][moveNum] = 0;
            } else {
                LMR_TABLE[depth][moveNum] = (int)(LMR_BASE + std::log(depth) * std::log(moveNum) / LMR_DIVISION);
            }
        }
    }
}

void resetNodeCounter() {
    nodeCount.store(0, std::memory_order_relaxed);
}

long long getNodeCounter() {
    return nodeCount.load(std::memory_order_relaxed);
}

// Start TIME CONTROL (& tt) params
std::atomic<bool> stop_search(false);
std::atomic<long long> soft_time_limit_ms{0};
std::atomic<long long> hard_time_limit_ms{0};
std::atomic<long long> start_time_ms{0};          // Start time in milliseconds since epoch (atomic for thread-safety)
std::atomic<bool> is_time_limited{false};         // Do we have time limit? (atomic for thread-safety)
std::atomic<bool> use_tt(true);
// End TIME CONTROL params

static std::string move_to_uci(const Move& m);

// HELPERS FOR ROOT SEARCH (52-268)
namespace {
    struct SearchSession{
        Board& board;
        int maxDepth;
        int movetimeMs;
        int ply;
        const SearchParams& params;
        std::vector<Move> possibleMoves;
        Move bestMoveSoFar;
        std::vector<uint64_t> localPositionHistory;
        int lastScore;
        std::chrono::steady_clock::time_point searchStart;
        bool timeLimited;
        int effectiveMaxDepth;
    };


void configure_time_limit(const int& timeToThink) {
    auto now = std::chrono::steady_clock::now();
    start_time_ms.store(std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count(), std::memory_order_relaxed);
    if (timeToThink > 0) {
        is_time_limited.store(true, std::memory_order_relaxed);
        long long time_limit = timeToThink; // Do not use the entire time given by the GUI leave a small margin
        if (time_limit > 50) time_limit -= 20; // 20ms safety margin
        soft_time_limit_ms.store(time_limit * 0.7, std::memory_order_relaxed);
        hard_time_limit_ms.store(time_limit, std::memory_order_relaxed);
    } else {
        is_time_limited.store(false, std::memory_order_relaxed);
        soft_time_limit_ms.store(0, std::memory_order_relaxed);
        hard_time_limit_ms.store(0, std::memory_order_relaxed);
    }
}

void order_moves(const Board& board, std::vector<Move>& allMoves){
    std::sort(allMoves.begin(), allMoves.end(), [&](const Move& a, const Move& b) {
        return scoreMove(board, a, 0, nullptr) > scoreMove(board, b, 0, nullptr);
    }); // Order moves by heuristic scores so that we start with the most promising move for better alpha-beta performance and aspiration windows
}

std::vector<uint64_t> init_local_history(const std::vector<uint64_t>& positionHistory) {
    std::vector<uint64_t> localPositionHistory; // Local copy of position history for this search thread, used for repetition detection. We keep only the last 100 positions to save memory and because threefold repetition is unlikely beyond that.
    localPositionHistory.reserve(std::min((size_t)100, positionHistory.size()) + 1);
    auto startIt = (positionHistory.size() > 100) ? (positionHistory.end() - 100) : positionHistory.begin(); // Keep only last 100 entries
    localPositionHistory.assign(startIt, positionHistory.end());
    return localPositionHistory;
}

SearchSession init_search_session(
    Board& board,
    int maxDepth,
    int movetimeMs,
    const std::vector<uint64_t>& positionHistory,
    int ply,
    const SearchParams& params
) {
    SearchSession session{
        board,
        maxDepth,
        movetimeMs,
        ply,
        params,
        {},
        {},
        {},
        0,
        std::chrono::steady_clock::now(),
        (movetimeMs > 0),
        (movetimeMs > 0) ? 128 : maxDepth
    };

    configure_time_limit(session.movetimeMs);
    session.possibleMoves = get_all_moves(session.board, session.board.stm == WHITE);
    order_moves(session.board, session.possibleMoves);
    session.localPositionHistory = init_local_history(positionHistory);
    if (!session.possibleMoves.empty()) {
        session.bestMoveSoFar = session.possibleMoves[0];
    }

    return session;
}

void reorder_with_pv(const Board& board, std::vector<Move>& possibleMoves, const Move& bestMoveSoFar) {
    std::stable_sort(possibleMoves.begin(), possibleMoves.end(), [&](const Move& a, const Move& b) {
        const bool aIsPV = (move_from(a) == move_from(bestMoveSoFar) && move_to(a) == move_to(bestMoveSoFar) && get_promotion_type(a) == get_promotion_type(bestMoveSoFar));
        const bool bIsPV = (move_from(b) == move_from(bestMoveSoFar) && move_to(b) == move_to(bestMoveSoFar) && get_promotion_type(b) == get_promotion_type(bestMoveSoFar));

        // Strict-weak-ordering: comparator must be irreflexive (never a < a).
        if (aIsPV != bIsPV) return aIsPV;

        const int sa = scoreMove(board, a, 0, nullptr);
        const int sb = scoreMove(board, b, 0, nullptr);
        if (sa != sb) return sa > sb;

        // Deterministic tie-breaker.
        if (sq_to_row(move_from(a)) != sq_to_row(move_from(b))) return sq_to_row(move_from(a)) < sq_to_row(move_from(b));
        if (sq_to_col(move_from(a)) != sq_to_col(move_from(b))) return sq_to_col(move_from(a)) < sq_to_col(move_from(b));
        if (sq_to_row(move_to(a)) != sq_to_row(move_to(b))) return sq_to_row(move_to(a)) < sq_to_row(move_to(b));
        if (sq_to_col(move_to(a)) != sq_to_col(move_to(b))) return sq_to_col(move_to(a)) < sq_to_col(move_to(b));
        return get_promotion_type(a) < get_promotion_type(b);
    });
}

void search_moves_for_one_depth(
    Board& board,
    const std::vector<Move>& possibleMoves,
    int depth,
    int& alpha,
    int beta,
    int ply,
    std::vector<uint64_t>& localPositionHistory,
    int& bestValue,
    Move& currentDepthBestMove,
    std::vector<Move>& bestPvForDepth
) {
    for (Move move : possibleMoves) {
        if (stop_search.load(std::memory_order_relaxed)) {
            break;
        }

        board.makeMove(move);

        std::vector<Move> childPv;
        uint64_t newHash = position_key(board);
        localPositionHistory.push_back(newHash);
        int val = -negamax(board, depth - 1, -beta, -alpha, ply + 1, localPositionHistory, childPv);
        localPositionHistory.pop_back();

        board.unmakeMove(move);

        if (stop_search.load(std::memory_order_relaxed)) {
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
}
void emit_info_line(
    int depth,
    int bestValue,
    const std::vector<Move>& bestPvForDepth,
    const std::chrono::steady_clock::time_point& searchStart
) {
    auto searchEnd = std::chrono::steady_clock::now();
    long long duration = std::chrono::duration_cast<std::chrono::milliseconds>(searchEnd - searchStart).count();
    std::cout << "info depth " << depth << " ";
    long long nps = 0;
    if (duration > 0) {
        nps = (getNodeCounter() * 1000LL) / duration;
    }
    if (std::abs(bestValue) >= MATE_SCORE - 1000) {
        int mateIn = (MATE_SCORE - std::abs(bestValue) + 1) / 2;
        if (bestValue < 0) mateIn = -mateIn;
        std::cout << "score mate " << mateIn;
    } else {
        std::cout << "score cp " << bestValue;
    }
    std::cout << " time " << duration
                << " nps " << nps
                << " pv ";
    for (const Move& pvMove : bestPvForDepth) {
        std::cout << move_to_uci(pvMove) << " ";
    }
    std::cout << std::endl;
}

int search_one_depth_with_aspiration(
    Board& board,
    const SearchParams& params,
    std::vector<Move>& possibleMoves,
    int depth,
    int ply,
    std::vector<uint64_t>& localPositionHistory,
    Move& bestMoveSoFar,
    int& lastScore,
    const std::chrono::steady_clock::time_point& searchStart
) {
    int delta = params.aspiration_delta; // Aspiration window margin
    int alpha = -VALUE_INF;
    int beta = VALUE_INF;
    int bestValue = std::numeric_limits<int>::min() / 2;

    if (params.use_aspiration && depth >= 5) {
        alpha = std::max(-VALUE_INF, lastScore - delta);
        beta = std::min(VALUE_INF, lastScore + delta);
    }

    while (true) {
        const int alphaStart = alpha;
        const int betaStart = beta;
        bestValue = std::numeric_limits<int>::min() / 2;
        Move currentDepthBestMove; // Only the best move found at this depth

        if (depth > 1) {
            reorder_with_pv(board, possibleMoves, bestMoveSoFar);
        }

        std::vector<Move> bestPvForDepth;
        search_moves_for_one_depth(
            board,
            possibleMoves,
            depth,
            alpha,
            beta,
            ply,
            localPositionHistory,
            bestValue,
            currentDepthBestMove,
            bestPvForDepth
        );

        if (stop_search.load(std::memory_order_relaxed)) {
            break;
        }

        if (params.use_aspiration && depth >= 5 && (bestValue <= alphaStart || bestValue >= betaStart)) {
            alpha = std::max(-VALUE_INF, bestValue - delta);
            beta = std::min(VALUE_INF, bestValue + delta);
            delta += delta / 2;
            continue;
        }

        if (bestValue > std::numeric_limits<int>::min() / 2) {
            bestMoveSoFar = currentDepthBestMove;
            lastScore = bestValue;
            emit_info_line(depth, bestValue, bestPvForDepth, searchStart);
        }
        break;
    }

    return bestValue;
}
}

// HELPERS END HERE

namespace {
SearchParams g_search_params{};
}

const SearchParams& get_search_params() {
    return g_search_params;
}

void set_search_params(const SearchParams& params) {
    g_search_params = params;
}

void request_stop_search() {
    stop_search.store(true, std::memory_order_relaxed);
}

void set_use_tt(bool enabled) {
    use_tt.store(enabled, std::memory_order_relaxed);
}

void clear_search_heuristics() {
    clear_history();
}

static bool is_square_attacked_otf(const Board& board, int row, int col, bool byWhite) {
    int sq = row_col_to_sq(row, col);
    const int us = byWhite ? WHITE : BLACK;
    Bitboard occ = board.color[WHITE] | board.color[BLACK];

    Bitboard pawns = board.piece[PAWN - 1] & board.color[us];
    Bitboard knights = board.piece[KNIGHT - 1] & board.color[us];
    Bitboard bishops = board.piece[BISHOP - 1] & board.color[us];
    Bitboard rooks = board.piece[ROOK - 1] & board.color[us];
    Bitboard queens = board.piece[QUEEN - 1] & board.color[us];
    Bitboard kings = board.piece[KING - 1] & board.color[us];

    if (byWhite) {
        if (pawn_attacks[BLACK][sq] & pawns) return true;
    } else {
        if (pawn_attacks[WHITE][sq] & pawns) return true;
    }

    if (knight_attacks[sq] & knights) return true;
    if (king_attacks[sq] & kings) return true;

    if (bishop_attacks_on_the_fly(sq, occ) & (bishops | queens)) return true;
    if (rook_attacks_on_the_fly(sq, occ) & (rooks | queens)) return true;

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
            if (elapsed >= hard_time_limit_ms.load(std::memory_order_relaxed)) {
                stop_search.store(true, std::memory_order_relaxed);
                return true;
            }
        }
    }
    return false;
}

static std::string move_to_uci(const Move& m) {
    std::string s;
    s += columns[sq_to_col(move_from(m))];
    s += static_cast<char>('0' + (8 - sq_to_row(move_from(m))));
    s += columns[sq_to_col(move_to(m))];
    s += static_cast<char>('0' + (8 - sq_to_row(move_to(m))));
    if (get_promotion_type(m) != -1) {
        switch (get_promotion_type(m)) {
            case QUEEN: s += 'q'; break;
            case ROOK: s += 'r'; break;
            case BISHOP: s += 'b'; break;
            case KNIGHT: s += 'n'; break;
            default: break;
        }
    }
    return s;
}

const int SEE_MOVE_ORDERING_THRESHOLD = -82; // ~minus pawn

int scoreMove(const Board& board, const Move& move, int ply, const Move* ttMove) {
    int moveScore = 0;
    int flags = move_flags(move);
    int from = move_from(move);
    int to = move_to(move);
    if (is_capture(move)) {
        int victimPiece = flags == FLAG_EN_PASSANT ? PAWN : piece_type(board.mailbox[to]);
        int victimValue = PIECE_VALUES[victimPiece];

        int attackerPiece = piece_at_sq(board, from);
        int attackerValue = PIECE_VALUES[piece_type(attackerPiece)];
        int mvvScore = victimValue * 10 - attackerValue;

        // Use threshold-based SEE for move ordering
        if (staticExchangeEvaluation(board, move, SEE_MOVE_ORDERING_THRESHOLD)) {
            // Good trades (passes threshold). they must be on the top of the list.
            moveScore += SCORE_GOOD_CAPTURE + mvvScore;
        }
        else {
            // Bad trades (fails threshold). apply penalty to push them down the list.
            moveScore += SCORE_BAD_CAPTURE + mvvScore;
        }

        moveScore += 10000 + (victimValue * 10) - attackerValue;
    }

    if (ttMove != nullptr && moves_equal(move, *ttMove)) {
        moveScore += SCORE_TT_MOVE; // TT move always has the highest priority
    }

    if (get_promotion_type(move) != -1) {
        switch (get_promotion_type(move)){
            case QUEEN: moveScore += SCORE_PROMO_QUEEN; break;
            case ROOK: moveScore += SCORE_PROMO_ROOK; break;
            case BISHOP: moveScore += SCORE_PROMO_BISHOP; break;
            case KNIGHT: moveScore += SCORE_PROMO_KNIGHT; break;
            default: break;
        }
    }

    if (move_flags(move) == FLAG_CASTLE_KING || move_flags(move) == FLAG_CASTLE_QUEEN) {
        // Castling is good for KING safety, but keep the bonus modest so we don't prefer it over
        // urgent defensive moves (like saving a hanging piece) at shallow depth
        moveScore += 500;
    }

    if (get_history_score(board.stm == WHITE ? 0 : 1, from, to) != 0) {
        moveScore += get_history_score(board.stm == WHITE ? 0 : 1, from, to);
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
    
    // Draw detection in quiescence
    if (is_fifty_move_draw(board)) {
        return 0;
    }
    if (is_insufficient_material(board)) {
        return 0;
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

    const SearchParams& params = get_search_params();
    for (Move& move : captureMoves) {
        if (params.use_qsearch_see) {
            // Use threshold-based SEE with threshold 0 (must not lose material)
            if (!staticExchangeEvaluation(board, move, 0)) {
                continue; // Bad capture, skip it
            }
        }

        // Delta Pruning
        // If even the most optimistic evaluation (stand_pat + value of captured piece + margin) is worse than alpha, skip 
        int capturedValue = PIECE_VALUES[piece_type(move_flags(move) == FLAG_EN_PASSANT ? PAWN : board.mailbox[move_to(move)])];
        if (stand_pat + capturedValue + 200 < alpha) {
            continue; 
        }

        board.makeMove(move);
        int kingRow = 0;
        int kingCol = 0;
        bool checkBlackKing = board.stm == WHITE;
        if (!king_square(board, !checkBlackKing, kingRow, kingCol)) {
            board.unmakeMove(move);
            continue;
        }
        
        if (is_square_attacked(board, kingRow, kingCol, board.stm == WHITE)) {
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
int negamax(Board& board, int depth, int alpha, int beta, int ply, std::vector<uint64_t>& positionHistory, std::vector<Move>& pvLine) {

    Move badQuiets[256]; // Store bad quiet moves for move ordering
    int badQuietCount = 0;

    nodeCount.fetch_add(1, std::memory_order_relaxed);

    if (should_stop()) {
        return 0; // Search was stopped
    }

    const SearchParams& params = get_search_params();
    bool pvNode = (beta - alpha) > 1;
    bool firstMove = true;
    const int alphaOrig = alpha;
    int maxEval = VALUE_NONE;

    int kRow = 0;
    int kCol = 0;
    if (!king_square(board, board.stm == WHITE, kRow, kCol)) {
        pvLine.clear();
        return 0;
    }
    bool inCheck = is_square_attacked(board, kRow, kCol, !(board.stm == WHITE));

    if (inCheck) {
        depth++; // Check extension
    }

    if (depth <= 0) {
        pvLine.clear();
        return quiescence(board, alpha, beta, ply);
    }

    // Draw detection: threefold repetition
    if (is_threefold_repetition(positionHistory)) {
        pvLine.clear();
        return 0; // Draw
    }
    
    // Draw detection: 50-move rule
    if (is_fifty_move_draw(board)) {
        pvLine.clear();
        return 0; // Draw
    }
    
    // Draw detection: insufficient material
    if (is_insufficient_material(board)) {
        pvLine.clear();
        return 0; // Draw
    }

    uint64_t currentHash = board.hash;
    int ttScore = 0;
    int ttDepth = 0;
    TTFlag ttFlag = TTFlag::EXACT;
    Move ttMove;
    bool ttHit = false;
    if (use_tt.load(std::memory_order_relaxed)) {
        ttHit = globalTT.probe(currentHash, ttScore, ttDepth, ttFlag, ttMove);
    }

    int movesSearched = 0;
    int eval = -MATE_SCORE;
    bool is_repetition_candidate = false;

    if (positionHistory.size() > 1) {
        for (int i = static_cast<int>(positionHistory.size()) - 2; i >= 0; i--) {
            if (positionHistory[i] == currentHash) {
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
        int kingRow = 0;
        int kingCol = 0;
        if (!king_square(board, board.stm == WHITE, kingRow, kingCol)) {
            pvLine.clear();
            return 0;
        }
        // Check if side to move is currently in check
        bool inCheck = is_square_attacked(board, kingRow, kingCol, !(board.stm == WHITE));

        if (!inCheck && depth >= 3 && (beta - alpha == 1)) {
            // Make a "null move" by flipping side to move
            const int prevEnPassantCol = board.enPassant;
            const uint64_t prevHash = board.hash; // backup hash before null move

            board.enPassant = -1; // En passant rights vanish after a null move.
            board.stm = other_color(board.stm);

            board.hash ^= zobrist().side; // stm changed
            
            // DÜZELTME: board.enPassant değil, yedeklediğimiz prevEnPassantCol kullanıyoruz!
            // DÜZELTME: Hafıza taşmasını engellemek için % 8 ekliyoruz!
            int oldEp = (prevEnPassantCol != -1) ? (prevEnPassantCol % 8) : 8;
            board.hash ^= zobrist().epFile[oldEp];
            board.hash ^= zobrist().epFile[8];

            positionHistory.push_back(board.hash);

            // Reduction factor R (typical values 2..3). Ensure we don't search negative depth
            int R = std::min(3, std::max(1, depth - 2));
            std::vector<Move> nullPv;
            int nullScore = -negamax(board, depth - 1 - R, -beta, -beta + 1, ply + 1, positionHistory, nullPv);

            // Undo positionHistory change and null move
            if (!positionHistory.empty()) positionHistory.pop_back();
            board.stm = other_color(board.stm);
            board.enPassant = prevEnPassantCol;
            board.hash = prevHash; // restore hash to before null move

            if (nullScore >= beta) {
                pvLine.clear();
                return beta; // Null-move cutoff
            }
        }
    }

    std::vector<Move> possibleMoves = get_all_moves(board, board.stm == WHITE);
    Move bestMove = possibleMoves.empty() ? Move() : possibleMoves[0];

    if (possibleMoves.empty()) {
        int kingRow = 0;
        int kingCol = 0;
        if (!king_square(board, board.stm == WHITE, kingRow, kingCol)) {
            return 0;
        }
        if (is_square_attacked(board, kingRow, kingCol, !(board.stm == WHITE))) 
            return -MATE_SCORE + ply; // Mate
        return 0; // Stalemate
    }

    // Move Ordering
    std::sort(possibleMoves.begin(), possibleMoves.end(), [&](const Move& a, const Move& b) {
        const Move* ttMovePtr = ttHit ? &ttMove : nullptr;
        return scoreMove(board, a, ply, ttMovePtr) > scoreMove(board, b, ply, ttMovePtr);
    });
    
    for (Move& move : possibleMoves) {

        // Futility Pruning
        if (depth < 3 && !inCheck && get_promotion_type(move) == -1 && is_quiet(move)) {
            int futilityMargin = 100 + 60 * depth; // Margin increases with depth
            if (staticEval + futilityMargin < alpha) {
                continue; // Skip this move, it's unlikely to raise the evaluation enough
            }

        }

        int lmpCount = (3 * depth * depth) + 4;
        // Late Move Pruning (LMP) logic
        if (params.use_lmp && !pvNode &&
            depth >= params.lmp_min_depth &&
            depth <= params.lmp_max_depth &&
            movesSearched >= lmpCount &&
            !inCheck && get_promotion_type(move) == -1 && piece_at_sq(board, move_to(move)) == EMPTY) {
            if (!(move_flags(move) == FLAG_EN_PASSANT)) {
                continue; // skip this move (late move pruning)
            }
        }

        // SEE PVS Pruning
        int seeThreshold = is_quiet(move) ? -67 * depth : -32 * depth * depth;
        if (movesSearched > 0 && !staticExchangeEvaluation(board, move, seeThreshold)) {
            continue;
        }

        board.makeMove(move);
        movesSearched++;
        std::vector<Move> childPv;
        uint64_t newHash = position_key(board);
        positionHistory.push_back(newHash);
        if (firstMove){
            eval = -negamax(board, depth - 1, -beta, -alpha, ply + 1, positionHistory, childPv);
            firstMove = false;
        }
        else {
            // Late Move Reduction (LMR)
            int reduction = 0;
            std::vector<Move> nullWindowPv;
            if (params.use_lmr &&
                depth > 1 && is_quiet(move)) {
                int lmrTableDepth = std::min(depth, 255);
                int lmrTableMovesSearched = std::min(movesSearched, 255);
                reduction = LMR_TABLE[lmrTableDepth][lmrTableMovesSearched]; // Increase reduction with depth
                if (reduction < 0) reduction = 0;
                if (reduction > depth - 1) reduction = depth - 1;
                if (depth - 1 - reduction < 1) reduction = depth - 2; // Ensure we dont search negative depth
            }
            int lmrDepth = std::max(0, depth - 1 - reduction);

            eval = -negamax(board, lmrDepth, -alpha - 1, -alpha, ply + 1, positionHistory, nullWindowPv);

            if (reduction > 0 && eval > alpha) {
                // Re-search at full depth if reduced search suggests a better move
                eval = -negamax(board, depth - 1, -alpha - 1, -alpha, ply + 1, positionHistory, childPv);
            }

            if (eval > alpha && eval < beta) {
                childPv.clear();
                eval = -negamax(board, depth - 1, -beta, -alpha, ply + 1, positionHistory, childPv);
            } else {
                childPv.clear();
            }
        }
        if (!positionHistory.empty()) positionHistory.pop_back();
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
            // Update history
            int from = move_from(move);
            int to = move_to(move);
            if (from >= 0 && from < 64 && to >= 0 && to < 64) {
                update_history(board.stm == WHITE ? 0 : 1, from, to, depth, badQuiets, badQuietCount);
            }
            
            break; // beta cutoff
        } else {
            if (is_quiet(move)) {
                if (badQuietCount < 256){
                    badQuiets[badQuietCount++] = move;
                }
            }
        }
    }

    TTFlag flag;
    if (maxEval <= alphaOrig) flag = ALPHA;
    else if (maxEval >= beta) flag = BETA;
    else flag = EXACT;
    
    if (use_tt.load(std::memory_order_relaxed)) {
        globalTT.store(currentHash, maxEval, depth, flag, bestMove);
    }
    return maxEval;
}


Move getBestMove(Board& board, int maxDepth, int movetimeMs, const std::vector<uint64_t>& positionHistory, int ply) {

    // Reset variables
    resetNodeCounter();
    stop_search.store(false, std::memory_order_relaxed);

    // Get search parameters (like LMR, aspiration windows, etc.) from the global settings
    const SearchParams& params = get_search_params();
    SearchSession session = init_search_session(board, maxDepth, movetimeMs, positionHistory, ply, params);

    if (session.possibleMoves.empty()) return {};
    if (session.possibleMoves.size() == 1) return session.possibleMoves[0];

    int bestValue = std::numeric_limits<int>::min() / 2; // Best score found at the current depth

    for (int depth = 1; depth <= session.effectiveMaxDepth; depth++) {

        if (stop_search.load(std::memory_order_relaxed)) break;

        bestValue = search_one_depth_with_aspiration(
            session.board,
            session.params,
            session.possibleMoves,
            depth,
            session.ply,
            session.localPositionHistory,
            session.bestMoveSoFar,
            session.lastScore,
            session.searchStart
        );
        
        if (stop_search.load(std::memory_order_relaxed)) {
            break;
        }

        if (session.timeLimited) {
            const auto now = std::chrono::steady_clock::now();
            const long long elapsed =
                std::chrono::duration_cast<std::chrono::milliseconds>(now - session.searchStart).count();
            if (elapsed >= soft_time_limit_ms.load(std::memory_order_relaxed)) {
                break;
            }
        }
    }

    return session.bestMoveSoFar; // Return the best move found within time/depth limits 
}
