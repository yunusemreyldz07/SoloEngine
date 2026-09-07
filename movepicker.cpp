#include "movepicker.h"

MovePicker::MovePicker(Board& b, Move tt, Move k1, Move k2, int p, bool qsearch)
    : board(b), ttMove(tt), ply(p), stage(STAGE_TT), moveCount(0), badCaptureCount(0), currentMoveIndex(0), isQSearch(qsearch) {
    killers[0] = k1;
    killers[1] = k2;
}

void MovePicker::score_captures() {
    for (int i = 0; i < moveCount; i++) {
        Move move = moves[i];
        int from = move_from(move);
        int to = move_to(move);
        int flags = move_flags(move);

        int victimPiece = flags == FLAG_EN_PASSANT ? PAWN : piece_type(board.mailbox[to]);
        int victimValue = PIECE_VALUES_MP[victimPiece];

        int attackerValue = PIECE_VALUES_MP[piece_type(piece_at_sq(board, from))];

        int score = victimValue * 10 - attackerValue;
        
        if (is_capture(move)) {
            score += get_capture_history(board, move);
        }

        if (ttMove != 0 && move == ttMove) {
            score += SCORE_TT_MOVE;
        }
        
        switch (get_promotion_type(move)) {
            case QUEEN:  score += SCORE_PROMO_QUEEN;  break;
            case ROOK:   score += SCORE_PROMO_ROOK;   break;
            case BISHOP: score += SCORE_PROMO_BISHOP; break;
            case KNIGHT: score += SCORE_PROMO_KNIGHT; break;
            default: break;
        }

        scores[i] = score;
    }
}

void MovePicker::score_quiets() {
    for (int i = 0; i < moveCount; i++) {
        Move move = moves[i];
        int from = move_from(move);
        int to = move_to(move);
        int piece = piece_at_sq(board, from);

        int score = 0;

        if (ttMove != 0 && move == ttMove) {
            score += SCORE_TT_MOVE;
        }

        int promo = get_promotion_type(move);

        if (ply < MAX_PLY && move == killers[0]) {
            score += SCORE_KILLER_1;
        } else if (ply < MAX_PLY && move == killers[1]) {
            score += SCORE_KILLER_2;
        } else if (promo == QUEEN){
            score += SCORE_PROMO_QUEEN;
        } else if (promo == ROOK) {
            score += SCORE_PROMO_ROOK;
        } else {
            score += get_history_score(board.stm, from, to);
            score += get_conhist_score(piece - 1, to, ply);

            if (promo == BISHOP) {
            score += SCORE_PROMO_BISHOP;
            } else if (promo == KNIGHT) {
                score += SCORE_PROMO_KNIGHT;
            }
        }

        scores[i] = score;
    }
}

Move MovePicker::next_scored_move() {
    if (currentMoveIndex >= moveCount) return 0;

    int bestScore = -9999999;
    int bestIndex = currentMoveIndex;

    for (int i = currentMoveIndex; i < moveCount; i++) {
        if (scores[i] > bestScore) {
            bestScore = scores[i];
            bestIndex = i;
        }
    }

    Move bestMove = moves[bestIndex];
    int score = scores[bestIndex];

    for (int i = bestIndex; i > currentMoveIndex; i--) {
        moves[i] = moves[i - 1];
        scores[i] = scores[i - 1];
    }

    moves[currentMoveIndex] = bestMove;
    scores[currentMoveIndex] = score;

    currentMoveIndex++;
    return bestMove;
}

bool MovePicker::has_moves() {
    Move pseudoMoves[256];
    int pseudoMoveCount = 0;
    generate_pseudo_moves(board, pseudoMoves, pseudoMoveCount);

    for (int i = 0; i < pseudoMoveCount; i++) {
        if (is_legal(pseudoMoves[i])) {
            return true;
        }
    }

    return false;
}

bool MovePicker::is_legal(Move move) {
    const bool sideToMove = board.stm == WHITE;

    board.makeMove(move);
    int kingSq = -1;
    king_square(board, sideToMove, kingSq);
    const bool legal = (kingSq != -1) && !is_square_attacked(board, kingSq, board.stm == WHITE);
    board.unmakeMove(move);

    return legal;
}

Move MovePicker::next_move() {
    while (true) {
        switch (stage) {
            case STAGE_TT:
                stage = STAGE_GEN_NOISY;
                if (ttMove != 0 && is_move_pseudo_legal(board, ttMove) && is_legal(ttMove)) {
                    return ttMove;
                }
                break;

            case STAGE_GEN_NOISY:
                moveCount = 0;
                currentMoveIndex = 0;
                generate_pseudo_captures(board, moves, moveCount);
                score_captures();
                stage = STAGE_GOOD_NOISY;
                break;

            case STAGE_GOOD_NOISY: {
                Move move = next_scored_move();
                if (move != 0) {
                    if (move == ttMove) continue;
                    if (!isQSearch && !staticExchangeEvaluation(board, move, 0)) {
                        if (badCaptureCount < 256) {
                            badCaptures[badCaptureCount++] = move;
                        }
                        continue;
                    }
                    if (!isQSearch && !is_legal(move)) {
                        continue;
                    }
                    return move;
                }

                if (isQSearch) {
                    stage = STAGE_DONE;
                } else {
                    stage = STAGE_GEN_QUIET;
                }
                break;
            }

            case STAGE_GEN_QUIET:
                moveCount = 0;
                currentMoveIndex = 0;
                generate_pseudo_quiets(board, moves, moveCount);
                score_quiets();
                stage = STAGE_QUIET;
                break;

            case STAGE_QUIET: {
                Move move = next_scored_move();
                if (move != 0) {
                    if (move == ttMove) continue;
                    if (!is_legal(move)) {
                        continue;
                    }
                    return move;
                }

                stage = STAGE_BAD_NOISY;
                currentMoveIndex = 0;
                break;
            }

            case STAGE_BAD_NOISY:
                if (currentMoveIndex < badCaptureCount) {
                    Move move = badCaptures[currentMoveIndex++];
                    if (move == ttMove) continue;
                    if (!is_legal(move)) {
                        continue;
                    }
                    return move;
                }
                stage = STAGE_DONE;
                break;

            case STAGE_DONE:
                return 0;
        }
    }
}
