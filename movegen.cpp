#include "board.h"
#include "bitboard.h"
#include "types.h"

#include <vector>

namespace {

inline void push_move(std::vector<Move>& moves, int fromSq, int toSq, bool isCapture = false, int promotion = 0, bool isEnPassant = false, bool isCastling = false) {
    int flag = MOVE_FLAG_NORMAL;
    if (isEnPassant) flag = MOVE_FLAG_EN_PASSANT;
    else if (isCastling) flag = MOVE_FLAG_CASTLING;
    else if (isCapture) flag = MOVE_FLAG_CAPTURE;
    moves.push_back(make_move(fromSq, toSq, promotion, flag));
}

inline int piece_on_square_bb(const Board& board, int sq) {
    const Bitboard mask = 1ULL << sq;
    if (board.color[WHITE] & mask) {
        for (int i = 0; i < 6; i++) {
            if (board.piece[i] & mask) return i + 1;
        }
        return 0;
    }
    if (board.color[BLACK] & mask) {
        for (int i = 0; i < 6; i++) {
            if (board.piece[i] & mask) return i + 7;
        }
        return 0;
    }
    return 0;
}

inline bool is_king_piece(int p) {
    return piece_type(p) == KING;
}

inline Bitboard board_occupancy(const Board& board) {
    return board.color[WHITE] | board.color[BLACK];
}

inline bool is_square_attacked_bb(const Board& board, int sq, bool byWhite) {
    const int us = byWhite ? WHITE : BLACK;
    const Bitboard occ = board_occupancy(board);

    const Bitboard pawns = board.piece[PAWN - 1] & board.color[us];
    const Bitboard knights = board.piece[KNIGHT - 1] & board.color[us];
    const Bitboard bishops = board.piece[BISHOP - 1] & board.color[us];
    const Bitboard rooks = board.piece[ROOK - 1] & board.color[us];
    const Bitboard queens = board.piece[QUEEN - 1] & board.color[us];
    const Bitboard kings = board.piece[KING - 1] & board.color[us];

    if (byWhite) {
        if (pawn_attacks[BLACK][sq] & pawns) return true;
    } else {
        if (pawn_attacks[WHITE][sq] & pawns) return true;
    }

    if (knight_attacks[sq] & knights) return true;
    if (king_attacks[sq] & kings) return true;
    if (get_bishop_attacks(sq, occ) & (bishops | queens)) return true;
    if (get_rook_attacks(sq, occ) & (rooks | queens)) return true;

    return false;
}

} // namespace

void generate_pawn_moves_bb(const Board& board, std::vector<Move>& moves) {
    const bool whiteToMove = (board.stm == WHITE);
    const int us = whiteToMove ? WHITE : BLACK;

    const Bitboard own = board.color[us];
    const Bitboard opp = board.color[whiteToMove ? BLACK : WHITE];
    Bitboard pawns = board.piece[PAWN - 1] & own;
    const Bitboard empty = ~(own | opp);

    while (pawns) {
        const int from = lsb(pawns);
        pawns &= pawns - 1;

        const int to = whiteToMove ? (from + 8) : (from - 8);
        if (to >= 0 && to < 64) {
            const Bitboard toMask = 1ULL << to;
            if (empty & toMask) {
                const bool isPromo = whiteToMove ? (from >= 48) : (from <= 15);
                if (isPromo) {
                    for (int promo : {QUEEN, ROOK, BISHOP, KNIGHT}) {
                        push_move(moves, from, to, false, promo, false, false);
                    }
                } else {
                    push_move(moves, from, to, false, 0, false, false);
                    const bool onStartRank = whiteToMove ? (from >= 8 && from <= 15) : (from >= 48 && from <= 55);
                    if (onStartRank) {
                        const int to2 = whiteToMove ? (from + 16) : (from - 16);
                        const Bitboard to2Mask = 1ULL << to2;
                        if (empty & to2Mask) {
                            push_move(moves, from, to2, false, 0, false, false);
                        }
                    }
                }
            }
        }

        Bitboard attacks = pawn_attacks[us][from] & opp;
        while (attacks) {
            const int capSq = lsb(attacks);
            attacks &= attacks - 1;
            const int captured = piece_on_square_bb(board, capSq);
            if (is_king_piece(captured)) continue;
            const bool isPromo = whiteToMove ? (capSq >= 56) : (capSq <= 7);
            if (isPromo) {
                for (int promo : {QUEEN, ROOK, BISHOP, KNIGHT}) {
                    push_move(moves, from, capSq, true, promo, false, false);
                }
            } else {
                push_move(moves, from, capSq, true, 0, false, false);
            }
        }

        if (board.enPassant != -1 && (pawn_attacks[us][from] & (1ULL << board.enPassant))) {
            push_move(moves, from, board.enPassant, true, 0, true, false);
        }
    }
}

void generate_knight_moves_bb(const Board& board, std::vector<Move>& moves) {
    const bool whiteToMove = (board.stm == WHITE);
    const int us = whiteToMove ? WHITE : BLACK;
    const int them = whiteToMove ? BLACK : WHITE;

    const Bitboard own = board.color[us];
    const Bitboard opp = board.color[them];
    Bitboard knights = board.piece[KNIGHT - 1] & own;

    while (knights) {
        const int from = lsb(knights);
        knights &= knights - 1;

        Bitboard targets = knight_attacks[from] & ~own;
        while (targets) {
            const int to = lsb(targets);
            targets &= targets - 1;
            const bool isCap = (opp & (1ULL << to)) != 0;
            if (isCap && is_king_piece(piece_on_square_bb(board, to))) continue;
            push_move(moves, from, to, isCap, 0, false, false);
        }
    }
}

void generate_bishop_moves_bb(const Board& board, std::vector<Move>& moves) {
    const bool whiteToMove = (board.stm == WHITE);
    const int us = whiteToMove ? WHITE : BLACK;
    const int them = whiteToMove ? BLACK : WHITE;

    const Bitboard own = board.color[us];
    const Bitboard opp = board.color[them];
    Bitboard bishops = board.piece[BISHOP - 1] & own;
    const Bitboard occ = board_occupancy(board);

    while (bishops) {
        const int from = lsb(bishops);
        bishops &= bishops - 1;

        Bitboard targets = get_bishop_attacks(from, occ) & ~own;
        while (targets) {
            const int to = lsb(targets);
            targets &= targets - 1;
            const bool isCap = (opp & (1ULL << to)) != 0;
            if (isCap && is_king_piece(piece_on_square_bb(board, to))) continue;
            push_move(moves, from, to, isCap, 0, false, false);
        }
    }
}

void generate_rook_moves_bb(const Board& board, std::vector<Move>& moves) {
    const bool whiteToMove = (board.stm == WHITE);
    const int us = whiteToMove ? WHITE : BLACK;
    const int them = whiteToMove ? BLACK : WHITE;

    const Bitboard own = board.color[us];
    const Bitboard opp = board.color[them];
    Bitboard rooks = board.piece[ROOK - 1] & own;
    const Bitboard occ = board_occupancy(board);

    while (rooks) {
        const int from = lsb(rooks);
        rooks &= rooks - 1;

        Bitboard targets = get_rook_attacks(from, occ) & ~own;
        while (targets) {
            const int to = lsb(targets);
            targets &= targets - 1;
            const bool isCap = (opp & (1ULL << to)) != 0;
            if (isCap && is_king_piece(piece_on_square_bb(board, to))) continue;
            push_move(moves, from, to, isCap, 0, false, false);
        }
    }
}

void generate_queen_moves_bb(const Board& board, std::vector<Move>& moves) {
    const bool whiteToMove = (board.stm == WHITE);
    const int us = whiteToMove ? WHITE : BLACK;
    const int them = whiteToMove ? BLACK : WHITE;

    const Bitboard own = board.color[us];
    const Bitboard opp = board.color[them];
    Bitboard queens = board.piece[QUEEN - 1] & own;
    const Bitboard occ = board_occupancy(board);

    while (queens) {
        const int from = lsb(queens);
        queens &= queens - 1;

        Bitboard targets = (get_bishop_attacks(from, occ) | get_rook_attacks(from, occ)) & ~own;
        while (targets) {
            const int to = lsb(targets);
            targets &= targets - 1;
            const bool isCap = (opp & (1ULL << to)) != 0;
            if (isCap && is_king_piece(piece_on_square_bb(board, to))) continue;
            push_move(moves, from, to, isCap, 0, false, false);
        }
    }
}

void generate_king_moves_bb(const Board& board, std::vector<Move>& moves) {
    const bool whiteToMove = (board.stm == WHITE);
    const int us = whiteToMove ? WHITE : BLACK;
    const int them = whiteToMove ? BLACK : WHITE;

    const Bitboard own = board.color[us];
    const Bitboard opp = board.color[them];
    const Bitboard kings = board.piece[KING - 1] & own;
    if (!kings) return;

    const int from = lsb(kings);

    Bitboard targets = king_attacks[from] & ~own;
    while (targets) {
        const int to = lsb(targets);
        targets &= targets - 1;
        const bool isCap = (opp & (1ULL << to)) != 0;
        if (isCap && is_king_piece(piece_on_square_bb(board, to))) continue;
        push_move(moves, from, to, isCap, 0, false, false);
    }

    const Bitboard occ = board_occupancy(board);
    const bool opponentIsWhite = !whiteToMove;

    if (whiteToMove && from == 4) {
        if (can_castle(board, CASTLE_WHITE_K)) {
            const Bitboard emptyMask = (1ULL << 5) | (1ULL << 6);
            const bool rookPresent = ((board.piece[ROOK - 1] & board.color[WHITE]) & (1ULL << 7)) != 0;
            if ((occ & emptyMask) == 0 &&
                !is_square_attacked_bb(board, 4, opponentIsWhite) &&
                !is_square_attacked_bb(board, 5, opponentIsWhite) &&
                !is_square_attacked_bb(board, 6, opponentIsWhite) &&
                rookPresent) {
                push_move(moves, 4, 6, false, 0, false, true);
            }
        }
        if (can_castle(board, CASTLE_WHITE_Q)) {
            const Bitboard emptyMask = (1ULL << 1) | (1ULL << 2) | (1ULL << 3);
            const bool rookPresent = ((board.piece[ROOK - 1] & board.color[WHITE]) & (1ULL << 0)) != 0;
            if ((occ & emptyMask) == 0 &&
                !is_square_attacked_bb(board, 4, opponentIsWhite) &&
                !is_square_attacked_bb(board, 3, opponentIsWhite) &&
                !is_square_attacked_bb(board, 2, opponentIsWhite) &&
                rookPresent) {
                push_move(moves, 4, 2, false, 0, false, true);
            }
        }
    }

    if (!whiteToMove && from == 60) {
        if (can_castle(board, CASTLE_BLACK_K)) {
            const Bitboard emptyMask = (1ULL << 61) | (1ULL << 62);
            const bool rookPresent = ((board.piece[ROOK - 1] & board.color[BLACK]) & (1ULL << 63)) != 0;
            if ((occ & emptyMask) == 0 &&
                !is_square_attacked_bb(board, 60, opponentIsWhite) &&
                !is_square_attacked_bb(board, 61, opponentIsWhite) &&
                !is_square_attacked_bb(board, 62, opponentIsWhite) &&
                rookPresent) {
                push_move(moves, 60, 62, false, 0, false, true);
            }
        }
        if (can_castle(board, CASTLE_BLACK_Q)) {
            const Bitboard emptyMask = (1ULL << 57) | (1ULL << 58) | (1ULL << 59);
            const bool rookPresent = ((board.piece[ROOK - 1] & board.color[BLACK]) & (1ULL << 56)) != 0;
            if ((occ & emptyMask) == 0 &&
                !is_square_attacked_bb(board, 60, opponentIsWhite) &&
                !is_square_attacked_bb(board, 59, opponentIsWhite) &&
                !is_square_attacked_bb(board, 58, opponentIsWhite) &&
                rookPresent) {
                push_move(moves, 60, 58, false, 0, false, true);
            }
        }
    }
}

bool is_square_attacked(const Board& board, int row, int col, bool isWhiteAttacker) {
    const int sq = row_col_to_sq(row, col);
    return is_square_attacked_bb(board, sq, isWhiteAttacker);
}

std::vector<Move> get_all_moves(Board& board, bool isWhiteTurn) {
    std::vector<Move> pseudoMoves;
    std::vector<Move> legalMoves;
    pseudoMoves.reserve(256);
    (void)isWhiteTurn;
    const bool sideToMove = (board.stm == WHITE);

    generate_pawn_moves_bb(board, pseudoMoves);
    generate_knight_moves_bb(board, pseudoMoves);
    generate_bishop_moves_bb(board, pseudoMoves);
    generate_rook_moves_bb(board, pseudoMoves);
    generate_queen_moves_bb(board, pseudoMoves);
    generate_king_moves_bb(board, pseudoMoves);

    for (Move m : pseudoMoves) {
        board.makeMove(m);
        int kingRow = 0;
        int kingCol = 0;
        if (!king_square(board, sideToMove, kingRow, kingCol)) {
            board.unmakeMove(m);
            continue;
        }
        if (!is_square_attacked(board, kingRow, kingCol, !sideToMove)) {
            legalMoves.push_back(m);
        }
        board.unmakeMove(m);
    }

    return legalMoves;
}

std::vector<Move> get_capture_moves(const Board& board) {
    std::vector<Move> moves;
    std::vector<Move> pseudoMoves;
    pseudoMoves.reserve(128);

    generate_pawn_moves_bb(board, pseudoMoves);
    generate_knight_moves_bb(board, pseudoMoves);
    generate_bishop_moves_bb(board, pseudoMoves);
    generate_rook_moves_bb(board, pseudoMoves);
    generate_queen_moves_bb(board, pseudoMoves);
    generate_king_moves_bb(board, pseudoMoves);

    for (Move m : pseudoMoves) {
        if (is_capture(m)) {
            moves.push_back(m);
        }
    }

    return moves;
}
