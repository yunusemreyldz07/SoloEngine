#include "search.h"
#include "evaluation.h"
#include "board.h"
#include <algorithm>
#include <chrono>
#include <limits>
#include <cstdlib>
#include <iostream>

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

    constexpr int MATE_SCORE = 100000;
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
        bool inCheck = is_square_attacked(board, board.isWhiteTurn ? board.whiteKingRow : board.blackKingRow, board.isWhiteTurn ? board.whiteKingCol : board.blackKingCol, !board.isWhiteTurn);

        // Futility pruning removed for quiet moves at low depth: it was cutting defensive moves
        // (like retreating a hanging piece) and led to nonsensical choices such as castling while
        // dropping material. Leaving the node unpruned keeps safety-first replies available.
        std::vector<Move> childPv;
        int eval = -negamax(board, depth - 1, -beta, -alpha, ply + 1, history, childPv);
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

int calculateTime(const Board& board, int wtime, int btime, int winc, int binc) {
    // All times are milliseconds (lichess protocol). Be conservative near flag, allow more when safe.
    if (wtime < 0 || btime < 0) {
        return 1500;
    }

    int myTime = board.isWhiteTurn ? wtime : btime;
    int oppTime = board.isWhiteTurn ? btime : wtime;
    int increment = board.isWhiteTurn ? winc : binc;

    // Emergency: low on time, think very fast.
    if (myTime <= 2000) {
        return std::max(80, myTime / 4);
    }

    int movesToGo = 40; // default a bit larger so each move gets less time
    double timeFactor = 1.0;

    // If far ahead on clock, use a bit more; if behind, tighten.
    if (myTime > oppTime * 2) {
        timeFactor = 1.25;
        movesToGo = 45;
    } else if (myTime < oppTime / 2) {
        timeFactor = 0.8;
        movesToGo = 35;
    }

    // Game phase adjustment.
    int pieceCount = 0;
    for (int r = 0; r < 8; r++) {
        for (int c = 0; c < 8; c++) {
            if (board.squares[r][c] != 0) pieceCount++;
        }
    }
    if (pieceCount <= 10) {
        timeFactor *= 1.15;
        movesToGo = std::max(25, movesToGo - 10);
    } else if (pieceCount >= 28) {
        timeFactor *= 0.85;
        movesToGo = std::min(50, movesToGo + 5);
    }

    bool inCheck = is_square_attacked(
        board,
        board.isWhiteTurn ? board.whiteKingRow : board.blackKingRow,
        board.isWhiteTurn ? board.whiteKingCol : board.blackKingCol,
        !board.isWhiteTurn
    );
    if (inCheck) {
        timeFactor *= 1.2;
        movesToGo = std::max(25, movesToGo - 5);
    }

    double baseTime = static_cast<double>(myTime) / movesToGo;
    double incBonus = increment * 0.6;

    int allocatedTime = static_cast<int>((baseTime + incBonus) * timeFactor);

    int minTime = std::max(80, increment / 2);
    int maxTime = myTime / 8; // keep 12.5% max to avoid flagging

    if (myTime < 15000) {
        maxTime = myTime / 10;
        minTime = 70;
    }

    allocatedTime = std::max(minTime, std::min(allocatedTime, maxTime));
    return allocatedTime;
}

// Iterative deepening is being added right now
Move getBestMove(Board& board, int maxDepth, const std::vector<uint64_t>& baseHistory, int wtime, int btime, int winc, int binc) {
    bool isWhite = board.isWhiteTurn;
    std::vector<Move> possibleMoves = get_all_moves(board, isWhite);
    std::sort(possibleMoves.begin(), possibleMoves.end(), [&](const Move& a, const Move& b) {
        return scoreMove(board, a, /*ply*/ 0) > scoreMove(board, b, /*ply*/ 0);
    });
    
    Move bestMove;
    if(!possibleMoves.empty()) bestMove = possibleMoves[0]; 

    std::vector<uint64_t> history = baseHistory;
    if (history.empty()) history.push_back(position_key(board));

    Move overallBestMove;
    std::vector<Move> bestPv;
    auto startTime = std::chrono::steady_clock::now();
    const bool timeLimited = (wtime >= 0 && btime >= 0);
    int allocatedTime = timeLimited ? calculateTime(board, wtime, btime, winc, binc)
                                    : std::numeric_limits<int>::max() / 4; // effectively no limit in depth mode
    for (int depth = 1; depth <= maxDepth; depth++) {
        if (timeLimited) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count();
            if (elapsed > allocatedTime) break;
        }
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

            std::vector<Move> childPv;
            int val = -negamax(board, depth - 1, -beta, -alpha, 1, history, childPv);
            history.pop_back();
            board.unmakeMove(move);
            
            if (val > bestValue) {
            bestValue = val;
            overallBestMove = move;
            bestPv.clear();
            bestPv.push_back(move);
            bestPv.insert(bestPv.end(), childPv.begin(), childPv.end());
            if (bestValue > alpha) {
                alpha = bestValue;
            }
            std::cout << "info score cp " << bestValue << " depth " << depth;
            if (!bestPv.empty()) {
                std::cout << " pv";
                for (const auto& mv : bestPv) {
                    std::cout << ' ' << move_to_uci(mv);
                }
            }
            std::cout << std::endl;
            }

            // Zaman koruması: tahsis edilen sürenin %120'sini geçerse dur
            if (timeLimited) {
                auto nowInner = std::chrono::steady_clock::now();
                auto elapsedInner = std::chrono::duration_cast<std::chrono::milliseconds>(nowInner - startTime).count();
                if (elapsedInner > static_cast<long long>(allocatedTime * 1.2)) {
                    break;
                }
            }
        }
        bestMove = overallBestMove;

        // Derinlik sonunda da kontrol et: tahsis edilen süreyi aşıyorsak çık
        if (timeLimited) {
            auto nowAfterDepth = std::chrono::steady_clock::now();
            auto elapsedAfterDepth = std::chrono::duration_cast<std::chrono::milliseconds>(nowAfterDepth - startTime).count();
            if (elapsedAfterDepth > static_cast<long long>(allocatedTime * 1.2)) {
                break;
            }
        }
    }
    return bestMove;
}