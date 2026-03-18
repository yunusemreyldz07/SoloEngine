#include "search.h"
#include "uci.h"
#include "bitboard.h"
#include "evaluation.h"
#include "board.h"
#include "history.h"

#include <atomic>
#include <iostream>
#include <vector>
#include <chrono>
#include <algorithm>
#include <cmath>

#define MAX_MOVES 256
#define SEE_THRESHOLD -82

namespace {
std::atomic<bool> stop_search{false};
std::atomic<long long> nodeCount{0};
std::atomic<long long> start_time_ms{0};
std::atomic<long long> soft_time_limit_ms{0};
std::atomic<long long> hard_time_limit_ms{0};
std::atomic<bool> time_limited{false};
std::atomic<int> seldepth{0};

inline void updateSeldepth(int ply){
    int cur = seldepth.load(std::memory_order_relaxed);
    while (ply > cur && !seldepth.compare_exchange_weak(cur, ply, std::memory_order_relaxed)) {
        // loop until we successfully update seldepth to the new value if ply is greater
    }
}

void resetSeldepth() {
    seldepth.store(0, std::memory_order_relaxed);
}

int getSeldepth() {
    return seldepth.load(std::memory_order_relaxed);
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
    if (stop_search.load(std::memory_order_relaxed)) return true;
    if (!time_limited.load(std::memory_order_relaxed)) return false;
    if ((nodeCount.load(std::memory_order_relaxed) & 2047) == 0) {
        long long elapsed = now_ms() - start_time_ms.load(std::memory_order_relaxed);
        if (elapsed >= hard_time_limit_ms.load(std::memory_order_relaxed)) {
            stop_search.store(true, std::memory_order_relaxed);
            return true;
        }
    }
    return false;
}

inline bool soft_limit_reached() {
    if (!time_limited.load(std::memory_order_relaxed)) return false;
    long long elapsed = now_ms() - start_time_ms.load(std::memory_order_relaxed);
    return elapsed >= soft_time_limit_ms.load(std::memory_order_relaxed);
}
}

int LMR_TABLE[256][256];
float LMR_BASE = 0.77f;
float LMR_DIVISION = 2.32f;

// Killer moves table: [ply][slot]
Move killerMoves[MAX_PLY][2];

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
    nodeCount.store(0, std::memory_order_relaxed);
}

long long getNodeCounter() {
    return nodeCount.load(std::memory_order_relaxed);
}

void requestSearchStop() {
    stop_search.store(true, std::memory_order_relaxed);
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
    if (stop_search.load(std::memory_order_relaxed)) return 0;
    nodeCount.fetch_add(1, std::memory_order_relaxed);
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
    orderMoves(board, captureMoves, moveCount, ttHit ? ttEntry.bestMove : 0, 0);
    int bestEval = stand_pat;
    Move bestMove = 0;

    for (int i = 0; i < moveCount; ++i) {
        Move captureMove = captureMoves[i];
        if (!staticExchangeEvaluation(board, captureMove, 0)) {
            continue; // Bad capture, skip it
        }
        board.makeMove(captureMove);
        
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

int16_t negamax(Board& board, int depth, int16_t alpha, int16_t beta, int ply, SearchStack* ss, std::vector<Move>& pvLine, std::vector<uint64_t>& positionHistory) {
    nodeCount.fetch_add(1, std::memory_order_relaxed);

    updateSeldepth(ply);

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

    if ((!ttHit || ttMove == 0 || ttEntry.depth < depth - 3) && depth >= 5) {
        depth--;
    }

    int16_t ttAdjustedEval = staticEval;
    if (ttHit && !inCheck) {
        if (ttEntry.flag == TT_EXACT ||
        (ttEntry.flag == TT_ALPHA && ttEntry.score >= staticEval) ||
        (ttEntry.flag == TT_BETA && ttEntry.score <= staticEval)) {
            ttAdjustedEval = ttEntry.score;
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
    
    orderMoves(board, moves, moveCount, ttMove, ply);
    Move bestMove = 0;

    // Reverse Futility Pruning
    if (!inCheck && !pvNode && depth < 9 && beta < MATE_SCORE - 100){

        int margin = 80 * depth;

        if (ttAdjustedEval - margin >= beta) {
            // I am so far ahead that even if I reduce my score by the margin, I am still above beta. No need to search this node.
            return ttAdjustedEval - margin;
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

        moveStack[ply] = {-1, -1}; // Sentinel for null move
        positionHistory.push_back(board.hash);

        int R = 3 + (depth / 3);
        std::vector<Move> nullPv;
        int16_t nullScore = -negamax(board, depth - R, -beta, -beta + 1, ply + 1, ss + 1, nullPv, positionHistory);
        
        positionHistory.pop_back();

        board.stm = other_color(board.stm);
        board.enPassant = prevEnPassant;
        board.hash = prevHash;

        if (nullScore >= beta) {
            pvLine.clear();
            return beta; // Null-move cutoff
        }
    }

    Move badQuiets[MAX_MOVES];
    int badQuietCount = 0;
    for (int movesSearched = 0; movesSearched < moveCount; ++movesSearched) {
        if (should_stop_search()) {
            aborted = true;
            break;
        }

        Move chosenMove = moves[movesSearched];
        bool isKiller = (ply < MAX_PLY && is_quiet(chosenMove) && 
                        (chosenMove == killerMoves[ply][0] || chosenMove == killerMoves[ply][1]));
        
        std::vector<Move> childPv; 
        
        // Futility Pruning (skip for killer moves)
        if (!isKiller && depth < 3 && !inCheck && get_promotion_type(chosenMove) == -1 && is_quiet(chosenMove)) {
            int futilityMargin = 100 + 60 * depth; // Margin increases with depth
            if (staticEval + futilityMargin < alpha) {
                continue; // Skip this move, it's unlikely to raise the evaluation enough
            }

        }

        int lmpCount = (3 * depth * depth) + 4;
        // Late Move Pruning (LMP) logic (skip for killer moves)
        if (!pvNode && !isKiller &&
            movesSearched >= lmpCount && is_quiet(chosenMove)) {
            continue; // skip this move (late move pruning)
        }
        
        // SEE PVS pruning (skip for killer moves)
        int seeThreshold = is_quiet(chosenMove) ? -67 * depth : -32 * depth * depth;
        if (movesSearched > 0 && !isKiller && !staticExchangeEvaluation(board, chosenMove, seeThreshold)) {
            continue;
        }

        moveStack[ply] = {board.mailbox[move_from(chosenMove)] - 1, move_to(chosenMove)};
        board.makeMove(chosenMove);

        positionHistory.push_back(board.hash); // Add new position to history for repetition detection
        if (firstMove){
            eval = -negamax(board, depth - 1, -beta, -alpha, ply + 1, ss + 1, childPv, positionHistory);
            firstMove = false;
        } else {

            int reduction = 0;

            if (depth > 1 && is_quiet(chosenMove)) {
                int lmrTableDepth = std::min(depth, 255);
                int lmrTableMovesSearched = std::min(movesSearched, 255);
                reduction = LMR_TABLE[lmrTableDepth][lmrTableMovesSearched]; // Increase reduction with depth
                if (isKiller) reduction--; // Reduce killer moves less
                if (reduction < 0) reduction = 0;
                if (reduction > depth - 1) reduction = depth - 1;
                if (depth - 1 - reduction < 1) reduction = depth - 2; // Ensure we dont search negative depth
            }

            int lmrDepth = std::max(0, depth - 1 - reduction);


            eval = -negamax(board, lmrDepth, -alpha - 1, -alpha, ply + 1, ss + 1, childPv, positionHistory); // PVS null window search
            
            if (reduction > 0 && eval > alpha) {
                // if the eval suggest a better move we research
                childPv.clear();
                eval = -negamax(board, depth - 1, -alpha - 1, -alpha, ply + 1, ss + 1, childPv, positionHistory); // Re-search with no reduction
            }
            if (eval > alpha && eval < beta) {
                // if we fail high search again with no reduction, and window
                childPv.clear();
                eval = -negamax(board, depth - 1, -beta, -alpha, ply + 1, ss + 1, childPv, positionHistory); // Re-search if we failed high
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
            pvLine.clear();
            pvLine.push_back(chosenMove);
            pvLine.insert(pvLine.end(), childPv.begin(), childPv.end());
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
    ttTable.writeEntry(hashKey, ttScore, static_cast<int8_t>(depth), flag, bestMove);


    return bestEval;
}

Move getBestMove(Board& board, int maxDepth, int movetimeMs, const std::vector<uint64_t>& positionHistory, int ply) {
    int16_t score = 0;

    SearchStack ss[MAX_PLY + 8] = {};

    std::vector<uint64_t> searchHistory = positionHistory;

    reset_movestack();
    stop_search.store(false, std::memory_order_relaxed);
    resetNodeCounter();
    clearKillers();
    const long long searchStartMs = now_ms();
    start_time_ms.store(searchStartMs, std::memory_order_relaxed);
    if (movetimeMs > 0) {
        long long safeTime = movetimeMs;
        if (safeTime > 50) safeTime -= 20;
        long long softTime = (safeTime * 7) / 10;
        if (softTime < 1) softTime = 1;
        soft_time_limit_ms.store(softTime, std::memory_order_relaxed);
        hard_time_limit_ms.store(safeTime, std::memory_order_relaxed);
        time_limited.store(true, std::memory_order_relaxed);
    } else {
        time_limited.store(false, std::memory_order_relaxed);
        soft_time_limit_ms.store(0, std::memory_order_relaxed);
        hard_time_limit_ms.store(0, std::memory_order_relaxed);
    }

    int moveCount = 0;
    Move moves[MAX_MOVES];
    get_all_moves(board, moves, moveCount);
    if (moveCount == 0) return 0;
    Move bestMoveSoFar = moves[0];

    // Iterative Deepening
    for(int iterativeDepth = 1; iterativeDepth <= maxDepth; ++iterativeDepth) {
        resetSeldepth();

        if (iterativeDepth > 1 && soft_limit_reached()) {
            stop_search.store(true, std::memory_order_relaxed);
            break;
        }
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
            int16_t searchScore = negamax(board, iterativeDepth, alpha, beta, ply, ss, pvLine, searchHistory);

            if (should_stop_search()) {
                score = searchScore;
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

        if (!pvLine.empty()) {
            bestMoveSoFar = pvLine[0];
        }

        long long elapsedMs = now_ms() - searchStartMs;
        if (elapsedMs < 0) elapsedMs = 0;
        long long nodes = getNodeCounter();
        long long nps = (elapsedMs > 0) ? (nodes * 1000 / elapsedMs) : (nodes * 1000);

        std::cout << "info depth " << iterativeDepth
                  << " seldepth " << getSeldepth()
                  << " hashfull " << hash_full()
                  << " time " << elapsedMs
                  << " nps " << nps
                  << " score cp " << score
                  << " pv ";
        for (Move m : pvLine) {
            std::cout << move_to_uci(m) << " ";
        }
        std::cout << std::endl;

        if (soft_limit_reached()) {
            stop_search.store(true, std::memory_order_relaxed);
            break;
        }
    }

    return bestMoveSoFar;
}

