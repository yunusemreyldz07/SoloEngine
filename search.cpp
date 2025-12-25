#include "search.h"
#include "evaluation.h"
#include "board.h"
#include <algorithm>
#include <chrono>
#include <limits>
#include <cstdlib>
#include <iostream>

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
        moveScore += 5000;
        return moveScore;
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
        return scoreMove(board, a, ply) > scoreMove(board, b, ply);
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
    uint64_t currentHash = position_key(board);

    TTEntry* ttEntry = probeTranspositionTable(currentHash, transpositionTable);
    if (ttEntry != nullptr && ttEntry->depth >= depth) {
        if (ttEntry->flag == EXACT) {
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

    constexpr int MATE_SCORE = 100000;
    const int alphaOrig = alpha;
    int legalMoveCount = 0;
    int maxEval = std::numeric_limits<int>::min() / 2;
    Move bestMove{};

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

        if (eval > maxEval) {
            maxEval = eval;
            bestMove = move;
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

int calculateTime(const Board& board, int wtime, int btime, int winc, int binc) {

    // Süre bilgisi yoksa konservatif bir varsayılan kullan
    if (wtime < 0 || btime < 0) {
        return 2000; // 2 saniye
    }
    
    int baseTime = board.isWhiteTurn ? wtime : btime;   // ms
    int increment = board.isWhiteTurn ? winc : binc;    // ms

    // İki parçalı strateji: (1) increment'in çoğu + (2) kalan süreden küçük pay
    double incPart = (increment > 0) ? increment * 0.80 : 0.0;

    int divisor;
    if (baseTime > 180000) {          // 3+ dakika
        divisor = 30;                 // ~%3.3
    } else if (baseTime > 120000) {   // 2-3 dk
        divisor = 26;                 // ~%3.8
    } else if (baseTime > 60000) {    // 1-2 dk
        divisor = 20;                 // %5
    } else if (baseTime > 30000) {    // 30-60 sn
        divisor = 14;                 // ~%7
    } else if (baseTime > 15000) {    // 15-30 sn
        divisor = 10;                 // 10%
    } else if (baseTime > 8000) {     // 8-15 sn
        divisor = 8;                  // 12.5%
    } else {                          // <8 sn, acil
        divisor = 6;                  // ~16%
    }
    double posPart = static_cast<double>(baseTime) / divisor;

    int allocatedTime = static_cast<int>(incPart + posPart);

    // Sert üst limitler: kalan sürenin en fazla %20'si, increment varsa en fazla 3x increment
    int capRemaining = baseTime / 5; // %20
    int capIncrement = (increment > 0) ? (increment * 3) : capRemaining;
    int maxTime = std::min(capRemaining, capIncrement);

    // Güvenlik tamponu: asla kalan süreden 800 ms'den fazla marj bırakmadan harcama
    if (baseTime > 1200) {
        maxTime = std::min(maxTime, baseTime - 800);
    } else if (baseTime > 300) {
        maxTime = std::min(maxTime, baseTime - 200);
    }

    // Alt ve üst sınırlar
    allocatedTime = std::max(allocatedTime, 400);      // en az 0.4 sn
    allocatedTime = std::min(allocatedTime, maxTime);  // dinamik üst limit
    allocatedTime = std::min(allocatedTime, 15000);    // mutlak üst limit 15 sn

    return allocatedTime;
}

// Iterative deepening is being added right now
Move getBestMove(Board& board, int maxDepth, const std::vector<uint64_t>& baseHistory, int wtime, int btime, int winc, int binc) {
    bool isWhite = board.isWhiteTurn;
    std::vector<Move> possibleMoves = get_all_moves(board, isWhite);
    int kingRow = isWhite ? board.whiteKingRow : board.blackKingRow;
    int kingCol = isWhite ? board.whiteKingCol : board.blackKingCol;
    std::sort(possibleMoves.begin(), possibleMoves.end(), [&](const Move& a, const Move& b) {
        return scoreMove(board, a, /*ply*/ 0) > scoreMove(board, b, /*ply*/ 0);
    });
    
    Move bestMove;
    if(!possibleMoves.empty()) bestMove = possibleMoves[0]; 

    std::vector<uint64_t> history = baseHistory;
    if (history.empty()) history.push_back(position_key(board));

    Move overallBestMove;
    auto startTime = std::chrono::steady_clock::now();
    int allocatedTime = calculateTime(board, wtime, btime, winc, binc);
    for (int depth = 1; depth <= maxDepth; depth++) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count();
        if (elapsed > allocatedTime) break;
        int bestValue = std::numeric_limits<int>::min() / 2; 
        int alpha = -2000000000;
        int beta = 2000000000;
        // (PV move ordering)
        if (depth > 1 && (overallBestMove.fromRow != 0 || overallBestMove.fromCol != 0 || 
                          overallBestMove.toRow != 0 || overallBestMove.toCol != 0)) {
            std::sort(possibleMoves.begin(), possibleMoves.end(), [&](const Move& a, const Move& b) {
                bool aIsPV = (a.fromRow == overallBestMove.fromRow && a.fromCol == overallBestMove.fromCol && a.toRow == overallBestMove.toRow && a.toCol == overallBestMove.toCol);
                bool bIsPV = (b.fromRow == overallBestMove.fromRow && b.fromCol == overallBestMove.fromCol && b.toRow == overallBestMove.toRow && b.toCol == overallBestMove.toCol);
                if (aIsPV) return true;
                if (bIsPV) return false;
                return scoreMove(board, a, /*ply*/ 0) > scoreMove(board, b, /*ply*/ 0);
            });
        }
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

            int val = -negamax(board, depth - 1, -beta, -alpha, 1, history);
            history.pop_back();
            board.unmakeMove(move);
            
            if (val > bestValue) {
            bestValue = val;
            overallBestMove = move;
            if (bestValue > alpha) {
                alpha = bestValue;
            }
            std::cout << "info score cp " << bestValue << " depth " << depth << std::endl;
            }

            // Zaman koruması: tahsis edilen sürenin %120'sini geçerse dur
            auto nowInner = std::chrono::steady_clock::now();
            auto elapsedInner = std::chrono::duration_cast<std::chrono::milliseconds>(nowInner - startTime).count();
            if (elapsedInner > static_cast<long long>(allocatedTime * 1.2)) {
                break;
            }
        }
        bestMove = overallBestMove;

        // Derinlik sonunda da kontrol et: tahsis edilen süreyi aşıyorsak çık
        auto nowAfterDepth = std::chrono::steady_clock::now();
        auto elapsedAfterDepth = std::chrono::duration_cast<std::chrono::milliseconds>(nowAfterDepth - startTime).count();
        if (elapsedAfterDepth > static_cast<long long>(allocatedTime * 1.2)) {
            break;
        }
    }
    return bestMove;
}