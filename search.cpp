#include "search.h"
#include "uci.h"
#include "bitboard.h"
#include "evaluation.h"
#include "board.h"
#include "history.h"
#include "nnue.h"
#include "movepicker.h"

#include <atomic>
#include <iostream>
#include <vector>
#include <chrono>
#include <algorithm>
#include <cmath>
#include <cstring>

#define MAX_MOVES 256
#define SEE_THRESHOLD -82

namespace {
// Global stop flag — set by UCI 'stop' command to interrupt all threads.
std::atomic<bool> stop_search_global{false};

// Per-thread search state — no synchronisation needed, each thread is independent.
thread_local bool      stop_search_local    = false;
thread_local long long nodeCount            = 0;
thread_local long long start_time_ms        = 0;
thread_local long long soft_time_limit_ms   = 0;
thread_local long long hard_time_limit_ms   = 0;
thread_local long long soft_node_limit      = -1;
thread_local bool      time_limited         = false;
thread_local int       seldepth             = 0;
thread_local Move      singularPvTable[MAX_PLY][MAX_PLY];
thread_local int       singularPvLength[MAX_PLY];

inline void updateSeldepth(int ply) {
    if (ply > seldepth) seldepth = ply;
}

void resetSeldepth() {
    seldepth = 0;
}

int getSeldepth() {
    return seldepth;
}

int hash_full(void) {
  int used = 0;
  int samples = 1000;

  for (int i = 0; i < samples; ++i) {
    if (ttTable.getEntry(i).hashKey != 0) {
      used++;
    }
  }

  return used;
}

const int PIECE_VALUES[7] = {0, 100, 320, 330, 500, 900, 20000};

inline long long now_ms() {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
}

inline bool should_stop_search() {
    if (stop_search_global.load(std::memory_order_relaxed)) return true;
    if (stop_search_local) return true;
    // Check soft node limit on every node (thread_local, no contention)
    if (soft_node_limit > 0 && nodeCount >= soft_node_limit) {
        stop_search_local = true;
        return true;
    }
    if (!time_limited) return false;
    if ((nodeCount & 2047) == 0) {
        long long elapsed = now_ms() - start_time_ms;
        if (elapsed >= hard_time_limit_ms) {
            stop_search_local = true;
            return true;
        }
    }
    return false;
}

inline bool soft_limit_reached() {
    if (!time_limited) return false;
    long long elapsed = now_ms() - start_time_ms;
    return elapsed >= soft_time_limit_ms;
}

inline bool soft_node_limit_reached() {
    if (soft_node_limit <= 0) return false;
    return nodeCount >= soft_node_limit;
}

} // namespace

void setSoftNodeLimit(long long nodes) {
    soft_node_limit = nodes;
}

int LMR_TABLE[256][256];
float LMR_BASE = 0.77f;
float LMR_DIVISION = 2.32f;

// Killer moves table: per-thread so datagen threads don't corrupt each other
thread_local Move killerMoves[MAX_PLY][2];

void clearKillers() {
    for (int i = 0; i < MAX_PLY; i++) {
        killerMoves[i][0] = 0;
        killerMoves[i][1] = 0;
    }
}

void updateKillers(int ply, Move move) {
    if (ply >= MAX_PLY) return;
    // Don't store duplicates
    if (killerMoves[ply][0] == move) return;
    killerMoves[ply][1] = killerMoves[ply][0];
    killerMoves[ply][0] = move;
}

void initLMRtables(){
    for(int depth = 0; depth < 256; depth++){
        for(int moveNum = 0; moveNum < 256; moveNum++){
            if (depth < 2 || moveNum < 4){
                LMR_TABLE[depth][moveNum] = 0;
            } else {
                LMR_TABLE[depth][moveNum] = (int)(LMR_BASE + std::log(depth) * std::log(moveNum) / LMR_DIVISION);
            }
        }
    }
}

void resetNodeCounter() {
    nodeCount = 0;
}

long long getNodeCounter() {
    return nodeCount;
}

void requestSearchStop() {
    stop_search_global.store(true, std::memory_order_relaxed);
}

int scoreMove(Board& board, const Move& move, Move ttMove = 0, int ply = 0) {
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

        if (staticExchangeEvaluation(board, move, SEE_THRESHOLD)) {
            mvvScore += SCORE_GOOD_CAPTURE;
        } else {
            mvvScore += SCORE_BAD_CAPTURE;
        }

        score += mvvScore;
    }

    if (ttMove != 0 && move == ttMove) {
        score += SCORE_TT_MOVE;
    }

    if (is_quiet(move)) {
        // Killer move bonus (only for quiet moves)
        if (ply < MAX_PLY && move == killerMoves[ply][0]) {
            score += SCORE_KILLER_1;
        } else if (ply < MAX_PLY && move == killerMoves[ply][1]) {
            score += SCORE_KILLER_2;
        } else {
            score += get_history_score(board.stm, from, to);
            score += get_conhist_score(piece - 1, to, ply);
        }
    }

    return score;
}

void orderMoves(Board& board, Move* moves, int moveCount, Move ttMove = 0, int ply = 0) {
    int scores[MAX_MOVES];
    for (int i = 0; i < moveCount; i++) {
        scores[i] = scoreMove(board, moves[i], ttMove, ply);
    }
    // Insertion sort with pre-computed scores
    for (int i = 1; i < moveCount; i++) {
        int tmpScore = scores[i];
        Move tmpMove = moves[i];
        int j = i - 1;
        while (j >= 0 && scores[j] < tmpScore) {
            scores[j + 1] = scores[j];
            moves[j + 1] = moves[j];
            j--;
        }
        scores[j + 1] = tmpScore;
        moves[j + 1] = tmpMove;
    }
}

int16_t qsearch(Board& board, int16_t alpha, int16_t beta, int ply, SearchStack* ss) {
    if (should_stop_search()) return 0;
    nodeCount++;
    updateSeldepth(ply);

    if (ply >= 128) {
        return evaluate_board(board);
    }

    int16_t originalAlpha = alpha;
    uint64_t hashKey = board.hash;
    TTEntry& ttEntry = ttTable.getEntry(hashKey);
    bool ttHit = (ttEntry.hashKey == hashKey);
    int16_t ttScore = 0;

    if (ttHit) {
        ttScore = ttEntry.score;

        if (ttScore >= MATE_SCORE - MAX_PLY) ttScore -= ply;
        else if (ttScore <= -MATE_SCORE + MAX_PLY) ttScore += ply;

        if (ttEntry.flag == TT_EXACT) return ttScore;
        if (ttEntry.flag == TT_BETA && ttScore <= alpha) return ttScore;
        if (ttEntry.flag == TT_ALPHA && ttScore >= beta) return ttScore;
    }

    int kingSq = -1;
    king_square(board, board.stm == WHITE, kingSq);
    bool isInCheck = (kingSq != -1) && (is_square_attacked(board, kingSq, board.stm != WHITE));

    int bestEval;
    if (isInCheck) {
        // A static evaluation is not a legal alternative while in check.  All
        // legal evasions must be searched; if there are none this is mate.
        bestEval = -VALUE_INF;
    } else {
        const int standPat = evaluate_board(board);

        if (standPat >= beta) {
            return standPat;
        }

        if (standPat > alpha) {
            alpha = standPat;
        }

        bestEval = standPat;
    }
    const int staticEval = ttHit ? ttEntry.score : evaluate_board(board);

    const Move ttMove = ttHit ? ttEntry.bestMove : 0;
    MovePicker mp(board, ttHit ? ttEntry.bestMove : 0, 0, 0, 0, 
        !isInCheck); // If the side to move is in check, apply check evasions

    Move bestMove = 0;
    Move captureMove;
    bool hasLegalEvasion = false;

    while ((captureMove = mp.next_move()) != 0) {

        if (captureMove != ttMove && !staticExchangeEvaluation(board, captureMove, 0) && !isInCheck) { // SEE QS PRUNING
            continue;
        }

        // QS Futility pruning: if the move is so bad that even with after adding the margin we are still below alpha, we can skip it. Only applies to quiet moves.
        int qsFutilityMargin = staticEval + 100;
        if (qsFutilityMargin < alpha) {
            continue;
        }

        board.makeMove(captureMove);
        
        int kingSq = -1;
        king_square(board, board.stm == BLACK, kingSq);
        if (kingSq != -1 && is_square_attacked(board, kingSq, board.stm == WHITE)) {
            board.unmakeMove(captureMove);
            continue;
        }

        hasLegalEvasion = true;
        
        int eval = -qsearch(board, -beta, -alpha, ply + 1, ss + 1);
        
        board.unmakeMove(captureMove);

        if (eval > bestEval) {
            bestEval = eval;
        }

        if (eval > alpha) {
            alpha = eval;
            bestMove = captureMove;
        }

        if (alpha >= beta) {
            break; // Beta cutoff
        }
    }

    if (isInCheck && !hasLegalEvasion) {
        bestEval = -MATE_SCORE + ply;
    }

    TTFlag flag = TT_EXACT;
    if (bestEval <= originalAlpha) {
        flag = TT_BETA;
    } else if (bestEval >= beta) {
        flag = TT_ALPHA;
    }

    int16_t ttStoreScore = bestEval;

    if (ttStoreScore >= MATE_SCORE - MAX_PLY) ttStoreScore += ply;
    else if (ttStoreScore <= -MATE_SCORE + MAX_PLY) ttStoreScore -= ply;

    ttTable.writeEntry(hashKey, ttStoreScore, 0, flag, bestMove);

    return bestEval;
}

int16_t negamax(Board& board, int depth, int16_t alpha, int16_t beta, int ply, SearchStack* ss, Move pvTable[][MAX_PLY], int pvLength[], std::vector<uint64_t>& positionHistory) {
    nodeCount++;

    const bool rootNode = (ply == 0);

    updateSeldepth(ply);

    pvLength[ply] = ply; // Initialize PV length for this ply
    if (should_stop_search()) return 0;
    
    if (depth <= 0) {
        return qsearch(board, alpha, beta, ply, ss);
    }

    // Check for repetition
    if (ply > 0 && (board.halfMoveClock >= 100 || is_repetition(positionHistory, board.halfMoveClock))){
        return 0; // DRAW
    }

    bool firstMove = true; // for PVS
    int16_t eval = 0; 

    const int16_t staticEval = evaluate_board(board);
    ss->staticEval = staticEval;
    ss->cutOffCount = 0;  // Initialize cutoff counter for this node
    const bool pvNode = (beta - alpha > 1);

    int kingSq = 0;
    king_square(board, board.stm == WHITE, kingSq);
    bool inCheck = is_square_attacked(board, kingSq, board.stm != WHITE);

    if (inCheck) {
        depth++; // Check extension
    }

    const bool improving = !inCheck && ply >= 2 && staticEval > (ss-2)->staticEval;

    int16_t originalAlpha = alpha;
    uint64_t hashKey = board.hash; 
    TTEntry& ttEntry = ttTable.getEntry(hashKey);
    Move ttMove = 0;
    bool ttHit = false;
    if (!ss->singularMove && ttEntry.hashKey == hashKey) {
        ttMove = ttEntry.bestMove;
        ttHit = true;
        if (ttEntry.depth >= depth && ply > 0) {
            int16_t ttScore = ttEntry.score;

            if (ttScore >= MATE_SCORE - MAX_PLY) {
                ttScore -= ply;
            } else if (ttScore <= -MATE_SCORE + MAX_PLY) {
                ttScore += ply;
            }

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

    // Internal Iterative Reductions (IIR)
    if ((!ttHit || ttMove == 0 || ttEntry.depth < depth - 3) && depth >= 4) {
        depth--;
    }



    MovePicker mp(board, ttMove, killerMoves[ply][0], killerMoves[ply][1], ply);
    Move bestMove = 0;

    if (!mp.has_moves()) {
        if (inCheck) {
            return -MATE_SCORE + ply;
        }
        return 0; // Stalemate
    }

    int ttAdjustedEval = staticEval;

    if (!ss->singularMove && ttMove && !inCheck &&
        (ttEntry.flag == TT_EXACT ||
         (ttEntry.flag == TT_ALPHA && ttEntry.score >= staticEval) ||
         (ttEntry.flag == TT_BETA && ttEntry.score <= staticEval))) {

        ttAdjustedEval = ttEntry.score;
    }

    // Reverse Futility Pruning
    if (!rootNode && !ss->singularMove && !inCheck && !pvNode && depth < 9 && beta < MATE_SCORE - 100){

        int margin = 80 * (depth - improving);

        if (staticEval - margin >= beta) {
            // I am so far ahead that even if I reduce my score by the margin, I am still above beta. No need to search this node.
            return staticEval - margin;
        }
    }

    // Razoring. if eval is far below alpha even the best capture, there's no point in searching. drop straight into qsearch.
    if (!pvNode && !rootNode && depth <= 3 && staticEval + 300 + 60 * depth < alpha){
        int16_t razoringScore = qsearch(board, alpha, beta, ply, ss);

        if (razoringScore < alpha) {
            return razoringScore;
        }
    }

    // Null Move Pruning
    if (!rootNode && !ss->singularMove && !ss->skipNullMove && !inCheck && depth >= 3 && !pvNode && ttAdjustedEval >= beta) {
        const int prevEnPassant = board.enPassant;
        const uint64_t prevHash = board.hash;

        board.enPassant = -1; // Clear EP square to prevent illegal null move
        board.stm = other_color(board.stm); // Switch side to move

        board.hash ^= zobrist().side; // Update hash for side to move change

        int oldEp = (prevEnPassant != -1) ? (prevEnPassant % 8) : 8;
        board.hash ^= zobrist().epFile[oldEp];
        board.hash ^= zobrist().epFile[8];

        moveStack[ply] = {-1, -1}; // Sentinel for null move
        positionHistory.push_back(board.hash);

        int R = 3 + (depth / 3);

        int16_t nullScore = -negamax(board, depth - R, -beta, -beta + 1, ply + 1, ss + 1, pvTable, pvLength, positionHistory);
        
        positionHistory.pop_back();

        board.stm = other_color(board.stm);
        board.enPassant = prevEnPassant;
        board.hash = prevHash;

        if (nullScore >= beta) {
            // Verification search to avoid zugzwang pitfalls
            if (depth >= 6) {
                const bool prevSkipNull = ss->skipNullMove;
                ss->skipNullMove = true;

                int verifyDepth = depth - R;
                if (verifyDepth < 1) verifyDepth = 1;

                int16_t verifyScore = negamax(board, verifyDepth, static_cast<int16_t>(beta - 1), beta, ply, ss, pvTable, pvLength, positionHistory);

                ss->skipNullMove = prevSkipNull;

                if (verifyScore >= beta) {
                    return beta;
                }
            } else {
                return beta; // Null move cutoff
            }
        }
    }

    int16_t bestEval = -VALUE_INF;
    bool aborted = false;
    
    Move badQuiets[MAX_MOVES];
    int badQuietCount = 0;
    pvLength[ply] = ply;

    Move chosenMove;
    int movesSearched = 0;

    while ((chosenMove = mp.next_move()) != 0) {

        if (should_stop_search()) {
            aborted = true;
            break;
        }

        const int moveIndex = movesSearched++;

        if (chosenMove == ss->singularMove) {
            continue;
        }

        bool isKiller = (ply < MAX_PLY && is_quiet(chosenMove) && (chosenMove == killerMoves[ply][0] || chosenMove == killerMoves[ply][1]));

        // Singular Extensions
        int extension = 0;
        bool trySingular = !(ply == 0) && (ply < depth * 2) && !ss->singularMove && ttHit && (chosenMove == ttMove) && depth >= 6 && (ttEntry.flag != TT_BETA) && (ttEntry.depth >= depth - 3) && abs(ttEntry.score) < MATE_SCORE - MAX_PLY;
        if (trySingular) {
            int16_t ttSeScore = ttEntry.score;
            if (ttSeScore >= MATE_SCORE - MAX_PLY) {
                ttSeScore -= ply;
            } else if (ttSeScore <= -MATE_SCORE + MAX_PLY) {
                ttSeScore += ply;
            }

            int singularBeta = ttSeScore - depth;
            const int singularDepth = (depth - 1) / 2;
        
            ss->singularMove = chosenMove;
            int16_t s = negamax(board, singularDepth, singularBeta - 1, singularBeta, ply, ss, singularPvTable, singularPvLength, positionHistory);
            ss->singularMove = 0;

            if (s < singularBeta) {
                extension = 1;
            }

            // Multicut
            else if (singularBeta >= beta) {
                return singularBeta;
            }

            // Negative Extensions
            else if (ttEntry.score >= beta) {
                extension = -1;
            }
        }


        
        // Futility Pruning (skip for killer moves)
        if (!rootNode && !isKiller && depth < 3 && !inCheck && get_promotion_type(chosenMove) == -1 && is_quiet(chosenMove)) {
            int futilityMargin = 100 + 60 * depth; // Margin increases with depth
            if (staticEval + futilityMargin < alpha) {
                continue; // Skip this move, it's unlikely to raise the evaluation enough
            }

        }

        int lmpCount = (3 * depth * depth + 4) / (2 - improving);
        // Late Move Pruning (LMP) logic (skip for killer moves)
        if (!rootNode && !pvNode && !isKiller &&
            moveIndex >= lmpCount && is_quiet(chosenMove)) {
            continue; // skip this move (late move pruning)
        }
        
        // SEE PVS pruning (skip for killer moves)
        int seeThreshold = is_quiet(chosenMove) ? -67 * depth : -32 * depth * depth;
        if (moveIndex > 0 && !isKiller && !staticExchangeEvaluation(board, chosenMove, seeThreshold)) {
            continue;
        }

        // History Pruning
        if (!rootNode && !pvNode && !inCheck && is_quiet(chosenMove) && moveIndex > 0 && depth <= 4) {
            int from = move_from(chosenMove);
            int to = move_to(chosenMove);
            int piece = board.mailbox[from] - 1;
            int histScore = get_history_score(board.stm, from, to) + get_conhist_score(piece, to, ply);
            if (histScore < -1024 * depth) {
                continue;
            }
        }

        moveStack[ply] = {board.mailbox[move_from(chosenMove)] - 1, move_to(chosenMove)};
        board.makeMove(chosenMove);

        // Legality check
        int kSq = -1;
        king_square(board, board.stm == BLACK, kSq);
        if (kSq != -1 && is_square_attacked(board, kSq, board.stm == WHITE)) {
            board.unmakeMove(chosenMove);
            continue;
        }

        positionHistory.push_back(board.hash); // Add new position to history for repetition detection
        const int fullDepth = depth - 1 + extension;
        if (firstMove){
            eval = -negamax(board, fullDepth, -beta, -alpha, ply + 1, ss + 1, pvTable, pvLength, positionHistory);
            firstMove = false;
        } else {

            int reduction = 0;

            if (depth > 1 && is_quiet(chosenMove)) {
                int lmrTableDepth = std::min(depth, 255);
                int lmrTableMovesSearched = std::min(moveIndex, 255);
                reduction = LMR_TABLE[lmrTableDepth][lmrTableMovesSearched]; // Increase reduction with depth
                if (isKiller) reduction--; // Reduce killer moves less
                if (reduction < 0) reduction = 0;
                if (reduction > depth - 1) reduction = depth - 1;
                if (depth - 1 - reduction < 1) reduction = depth - 2; // Ensure we dont search negative depth
            }

            int lmrDepth = std::max(0, fullDepth - reduction);


            eval = -negamax(board, lmrDepth, -alpha - 1, -alpha, ply + 1, ss + 1, pvTable, pvLength, positionHistory); // PVS null window search
            
            if (reduction > 0 && eval > alpha) {
                // if the eval suggest a better move we research
                eval = -negamax(board, fullDepth, -alpha - 1, -alpha, ply + 1, ss + 1, pvTable, pvLength, positionHistory); // Re-search with no reduction
            }
            if (eval > alpha && eval < beta) {
                // if we fail high search again with no reduction, and window
                eval = -negamax(board, fullDepth, -beta, -alpha, ply + 1, ss + 1, pvTable, pvLength, positionHistory); // Re-search if we failed high
            }
        }
        if (!positionHistory.empty()) positionHistory.pop_back();
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
            pvTable[ply][ply] = chosenMove;
            
            for (int j = ply + 1; j < pvLength[ply + 1]; j++) {
                pvTable[ply][j] = pvTable[ply + 1][j];
            }

            pvLength[ply] = pvLength[ply + 1];
        }

        if (alpha >= beta) {
            ss->cutOffCount++;  // Increment cutoff counter
            if (is_quiet(chosenMove)){
                update_history(board, board.stm, move_from(chosenMove), move_to(chosenMove), depth, badQuiets, badQuietCount, ply);
                updateKillers(ply, chosenMove);
            }
            break; // Beta cutoff
        }

        if (is_quiet(chosenMove)){
            if (badQuietCount < MAX_MOVES) {
                badQuiets[badQuietCount++] = chosenMove;
            }
        }
    }

    if (movesSearched == 0) {
        if (inCheck) {
            return -MATE_SCORE + ply;
        }
        return 0; // Stalemate
    }

    if (aborted) {
        return bestEval; // Don't write to TT if search was aborted.
    }

    TTFlag flag = TT_EXACT;
    if (alpha <= originalAlpha) {
        flag = TT_BETA;
    } else if (alpha >= beta) {
        flag = TT_ALPHA;
    }
    int16_t ttScore = bestEval;

    if (ttScore >= MATE_SCORE - MAX_PLY) {
        ttScore += ply;
    } else if (ttScore <= -MATE_SCORE + MAX_PLY) {
        ttScore -= ply;
    }
    
    if (!ss->singularMove) {
        ttTable.writeEntry(hashKey, ttScore, static_cast<int8_t>(depth), flag, bestMove);
    }


    return bestEval;
}

Move getBestMove(Board& board, int maxDepth, int movetimeMs, const std::vector<uint64_t>& positionHistory, int ply, bool silent, int16_t& outScore) {
    int16_t score = 0;
    outScore = 0;

    SearchStack ss[MAX_PLY + 8] = {};

    Move pvTable[MAX_PLY][MAX_PLY];
    int pvLength[MAX_PLY];
    memset(pvLength, 0, sizeof(pvLength));

    std::vector<uint64_t> searchHistory = positionHistory;

    reset_movestack();
    stop_search_local = false;
    stop_search_global.store(false, std::memory_order_relaxed); // clear any prior UCI stop
    resetNodeCounter();
    clearKillers();
    const long long searchStartMs = now_ms();
    start_time_ms = searchStartMs;
    if (movetimeMs > 0) {
        long long safeTime = movetimeMs;
        long long softTime = (safeTime * 7) / 10;
        if (softTime < 1) softTime = 1;
        soft_time_limit_ms = softTime;
        hard_time_limit_ms = safeTime;
        time_limited = true;
    } else {
        time_limited = false;
        soft_time_limit_ms = 0;
        hard_time_limit_ms = 0;
    }

    int moveCount = 0;
    Move moves[MAX_MOVES];
    get_all_moves(board, moves, moveCount);
    if (moveCount == 0) return 0;
    Move bestMoveSoFar = moves[0];

    // for smart time management
    int bestMoveStableDepths = 0;
    Move lastIterationBestMove = 0;
    double tmMultiplier = 1.0;

    // Iterative Deepening
    for(int iterativeDepth = 1; iterativeDepth <= maxDepth; ++iterativeDepth) {
        resetSeldepth();
        
        if (should_stop_search()) break;
        
        int16_t alpha = -VALUE_INF;
        int16_t beta = VALUE_INF;
        int delta = 9;
        const bool useAspiration = (iterativeDepth >= 4);

        if (useAspiration) {
            alpha = std::max<int16_t>(alpha, static_cast<int16_t>(score - delta));
            beta = std::min<int16_t>(beta, static_cast<int16_t>(score + delta));
        }

        while (true) {
            int16_t searchScore = negamax(board, iterativeDepth, alpha, beta, ply, ss, pvTable, pvLength, searchHistory);

            if (should_stop_search()) {
                break;
            }

            if (!useAspiration) {
                score = searchScore;
                break;
            }

            if (searchScore <= alpha) {
                delta = std::min(4096, delta + delta * 9 / 5);
                alpha = std::max<int16_t>(-VALUE_INF, static_cast<int16_t>(searchScore - delta));
                continue;
            }

            if (searchScore >= beta) {
                delta = std::min(4096, delta + delta * 9 / 5);
                beta = std::min<int16_t>(VALUE_INF, static_cast<int16_t>(searchScore + delta));
                continue;
            }

            score = searchScore;
            break;
        }

        if (should_stop_search()) {
            break;
        }

        if (pvLength[0] > 0) {
            bestMoveSoFar = pvTable[0][0];
        }

        long long elapsedMs = now_ms() - searchStartMs;
        if (elapsedMs < 0) elapsedMs = 0;
        long long nodes = getNodeCounter();
        long long nps = (elapsedMs > 0) ? (nodes * 1000 / elapsedMs) : (nodes * 1000);
        
        
        if (!silent) {
            if (score <= MATE_SCORE - MAX_PLY) {
                std::cout << "info depth " << iterativeDepth
                          << " seldepth " << getSeldepth()
                          << " nodes " << nodes
                          << " hashfull " << hash_full()
                          << " time " << elapsedMs
                          << " nps " << nps
                          << " score cp " << score
                          << " pv ";
            } else {
                std::cout << "info depth " << iterativeDepth
                          << " seldepth " << getSeldepth()
                          << " nodes " << nodes
                          << " hashfull " << hash_full()
                          << " time " << elapsedMs
                          << " nps " << nps
                          << " score mate " << (MATE_SCORE - score + 1) / 2
                          << " pv ";
            }
            for (int i = 0; i < pvLength[0]; i++) {
                std::cout << move_to_uci(pvTable[0][i]) << " ";
            }
            std::cout << std::endl;
        }

        if (pvLength[0] > 0 && iterativeDepth > 1) {
            Move currentBestMove = pvTable[0][0];
            
            // Move stability check
            if (currentBestMove == lastIterationBestMove) {
                bestMoveStableDepths++;
            } else {
                bestMoveStableDepths = 0; // 0 = Best move has changed
            }

            // define the tm multiplier
            tmMultiplier = 1.0;
            if (bestMoveStableDepths >= 3) {
                tmMultiplier *= 0.6; // move is stable. so use %60 
            }

            lastIterationBestMove = currentBestMove;
        }

        // dynamic soft limit calculation
        elapsedMs = now_ms() - searchStartMs;
        if (elapsedMs < 0) elapsedMs = 0;
        
        long long adjustedSoftTime = (long long)(soft_time_limit_ms * tmMultiplier);
        
        long long maxSafeTime = hard_time_limit_ms - 20;
        if (maxSafeTime < 1) maxSafeTime = 1;

        if (adjustedSoftTime > maxSafeTime) {
            adjustedSoftTime = maxSafeTime; 
        }

        // stop searching
        if (time_limited && iterativeDepth >= 3 && elapsedMs >= adjustedSoftTime) {
            stop_search_local = true;
            break;
        }



        if (soft_node_limit_reached()) {
            stop_search_local = true;
            break;
        }
    }

    outScore = score;
    return bestMoveSoFar;
}
