#include "board.h"
#include "bitboard.h"
#include "types.h"

#include <vector>

namespace {

inline Move make_move(int fromSq, int toSq, int capturedPiece = 0, int promotion = 0, bool isEnPassant = false, bool isCastling = false) {
    Move m;
    m.fromRow = sq_to_row(fromSq);
    m.fromCol = sq_to_col(fromSq);
    m.toRow = sq_to_row(toSq);
    m.toCol = sq_to_col(toSq);
    m.capturedPiece = capturedPiece;
    m.promotion = promotion;
    m.isEnPassant = isEnPassant;
    m.isCastling = isCastling;
    return m;
}

inline int capture_piece_at(const Board& board, int sq, int them) {
    Bitboard mask = 1ULL << sq;
    if ((board.color[them] & mask) == 0) return 0;
    if (board.piece[pawn - 1] & mask) return them == WHITE ? pawn : -pawn;
    if (board.piece[knight - 1] & mask) return them == WHITE ? knight : -knight;
    if (board.piece[bishop - 1] & mask) return them == WHITE ? bishop : -bishop;
    if (board.piece[rook - 1] & mask) return them == WHITE ? rook : -rook;
    if (board.piece[queen - 1] & mask) return them == WHITE ? queen : -queen;
    if (board.piece[king - 1] & mask) return them == WHITE ? king : -king;
    return 0;
}

inline bool is_king_piece(int piece) {
    return std::abs(piece) == king;
}

inline Bitboard board_occupancy(const Board& board) {
    return board.color[WHITE] | board.color[BLACK];
}

inline bool is_square_attacked_bb(const Board& board, int sq, bool byWhite) {
    const int us = byWhite ? WHITE : BLACK;
    Bitboard occ = board_occupancy(board);

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

    if (get_bishop_attacks(sq, occ) & (bishops | queens)) return true;
    if (get_rook_attacks(sq, occ) & (rooks | queens)) return true;

    return false;
}

/*
-----------------------------------
|         MOVE GENERATION         |
-----------------------------------
*/

template <typename Emit>
static inline void generate_moves_bb(const Board& board, Emit emit) {
    int source_square = 0;
    int target_square = 0;
    Bitboard bitboard = 0ULL;
    Bitboard attacks = 0ULL;

    const bool whiteToMove = board.isWhiteTurn;
    const int us = whiteToMove ? WHITE : BLACK;
    const int them = whiteToMove ? BLACK : WHITE;
    const Bitboard own = board.color[us];
    const Bitboard opp = board.color[them];
    const Bitboard occ = board_occupancy(board);

    // pawns
    bitboard = board.piece[pawn - 1] & own;
    while (bitboard) {
        source_square = lsb(bitboard);
        bitboard &= bitboard - 1;

        target_square = whiteToMove ? (source_square + 8) : (source_square - 8);
        if (target_square >= 0 && target_square < 64) {
            Bitboard toMask = 1ULL << target_square;
            if ((occ & toMask) == 0) {
                bool isPromo = whiteToMove ? (source_square >= 48) : (source_square <= 15);
                if (isPromo) {
                    for (int promo : {queen, rook, bishop, knight}) {
                        emit(make_move(source_square, target_square, 0, promo, false));
                    }
                } else {
                    emit(make_move(source_square, target_square));
                    bool onStartRank = whiteToMove ? (source_square >= 8 && source_square <= 15)
                                                   : (source_square >= 48 && source_square <= 55);
                    if (onStartRank) {
                        int to2 = whiteToMove ? (source_square + 16) : (source_square - 16);
                        Bitboard to2Mask = 1ULL << to2;
                        if ((occ & to2Mask) == 0) {
                            emit(make_move(source_square, to2));
                        }
                    }
                }
            }
        }

        attacks = pawn_attacks[us][source_square] & opp;
        while (attacks) {
            target_square = lsb(attacks);
            attacks &= attacks - 1;
            int captured = capture_piece_at(board, target_square, them);
            if (is_king_piece(captured)) continue;
            bool isPromo = whiteToMove ? (target_square >= 56) : (target_square <= 7);
            if (isPromo) {
                for (int promo : {queen, rook, bishop, knight}) {
                    emit(make_move(source_square, target_square, captured, promo, false));
                }
            } else {
                emit(make_move(source_square, target_square, captured, 0, false));
            }
        }

        if (board.enPassantCol != -1) {
            int epRow = whiteToMove ? 2 : 5;
            int epSq = row_col_to_sq(epRow, board.enPassantCol);
            if (pawn_attacks[us][source_square] & (1ULL << epSq)) {
                int captured = whiteToMove ? -pawn : pawn;
                emit(make_move(source_square, epSq, captured, 0, true));
            }
        }
    }

    // knights
    bitboard = board.piece[knight - 1] & own;
    while (bitboard) {
        source_square = lsb(bitboard);
        bitboard &= bitboard - 1;

        attacks = knight_attacks[source_square] & ~own;
        while (attacks) {
            target_square = lsb(attacks);
            attacks &= attacks - 1;
            int captured = (opp & (1ULL << target_square)) ? capture_piece_at(board, target_square, them) : 0;
            if (is_king_piece(captured)) continue;
            emit(make_move(source_square, target_square, captured, 0, false));
        }
    }

    // bishops
    bitboard = board.piece[bishop - 1] & own;
    while (bitboard) {
        source_square = lsb(bitboard);
        bitboard &= bitboard - 1;

        attacks = get_bishop_attacks(source_square, occ) & ~own;
        while (attacks) {
            target_square = lsb(attacks);
            attacks &= attacks - 1;
            int captured = (opp & (1ULL << target_square)) ? capture_piece_at(board, target_square, them) : 0;
            if (is_king_piece(captured)) continue;
            emit(make_move(source_square, target_square, captured, 0, false));
        }
    }

    // rooks
    bitboard = board.piece[rook - 1] & own;
    while (bitboard) {
        source_square = lsb(bitboard);
        bitboard &= bitboard - 1;

        attacks = get_rook_attacks(source_square, occ) & ~own;
        while (attacks) {
            target_square = lsb(attacks);
            attacks &= attacks - 1;
            int captured = (opp & (1ULL << target_square)) ? capture_piece_at(board, target_square, them) : 0;
            if (is_king_piece(captured)) continue;
            emit(make_move(source_square, target_square, captured, 0, false));
        }
    }

    // queens
    bitboard = board.piece[queen - 1] & own;
    while (bitboard) {
        source_square = lsb(bitboard);
        bitboard &= bitboard - 1;

        attacks = get_queen_attacks(source_square, occ) & ~own;
        while (attacks) {
            target_square = lsb(attacks);
            attacks &= attacks - 1;
            int captured = (opp & (1ULL << target_square)) ? capture_piece_at(board, target_square, them) : 0;
            if (is_king_piece(captured)) continue;
            emit(make_move(source_square, target_square, captured, 0, false));
        }
    }

    // kings
    bitboard = board.piece[king - 1] & own;
    if (bitboard) {
        source_square = lsb(bitboard);
        attacks = king_attacks[source_square] & ~own;
        while (attacks) {
            target_square = lsb(attacks);
            attacks &= attacks - 1;
            int captured = (opp & (1ULL << target_square)) ? capture_piece_at(board, target_square, them) : 0;
            if (is_king_piece(captured)) continue;
            emit(make_move(source_square, target_square, captured, 0, false));
        }

        const bool opponentIsWhite = !whiteToMove;
        if (whiteToMove && source_square == 4) {
            if (board.whiteCanCastleKingSide) {
                const Bitboard emptyMask = (1ULL << 5) | (1ULL << 6);
                const bool rookPresent = (board.piece[rook - 1] & board.color[WHITE]) & (1ULL << 7);
                if ((occ & emptyMask) == 0 &&
                    !is_square_attacked_bb(board, 4, opponentIsWhite) &&
                    !is_square_attacked_bb(board, 5, opponentIsWhite) &&
                    !is_square_attacked_bb(board, 6, opponentIsWhite) &&
                    rookPresent) {
                    emit(make_move(4, 6, 0, 0, false, true));
                }
            }
            if (board.whiteCanCastleQueenSide) {
                const Bitboard emptyMask = (1ULL << 1) | (1ULL << 2) | (1ULL << 3);
                const bool rookPresent = (board.piece[rook - 1] & board.color[WHITE]) & (1ULL << 0);
                if ((occ & emptyMask) == 0 &&
                    !is_square_attacked_bb(board, 4, opponentIsWhite) &&
                    !is_square_attacked_bb(board, 3, opponentIsWhite) &&
                    !is_square_attacked_bb(board, 2, opponentIsWhite) &&
                    rookPresent) {
                    emit(make_move(4, 2, 0, 0, false, true));
                }
            }
        }

        if (!whiteToMove && source_square == 60) {
            if (board.blackCanCastleKingSide) {
                const Bitboard emptyMask = (1ULL << 61) | (1ULL << 62);
                const bool rookPresent = (board.piece[rook - 1] & board.color[BLACK]) & (1ULL << 63);
                if ((occ & emptyMask) == 0 &&
                    !is_square_attacked_bb(board, 60, opponentIsWhite) &&
                    !is_square_attacked_bb(board, 61, opponentIsWhite) &&
                    !is_square_attacked_bb(board, 62, opponentIsWhite) &&
                    rookPresent) {
                    emit(make_move(60, 62, 0, 0, false, true));
                }
            }
            if (board.blackCanCastleQueenSide) {
                const Bitboard emptyMask = (1ULL << 57) | (1ULL << 58) | (1ULL << 59);
                const bool rookPresent = (board.piece[rook - 1] & board.color[BLACK]) & (1ULL << 56);
                if ((occ & emptyMask) == 0 &&
                    !is_square_attacked_bb(board, 60, opponentIsWhite) &&
                    !is_square_attacked_bb(board, 59, opponentIsWhite) &&
                    !is_square_attacked_bb(board, 58, opponentIsWhite) &&
                    rookPresent) {
                    emit(make_move(60, 58, 0, 0, false, true));
                }
            }
        }
    }
}
} // namespace

bool is_square_attacked(const Board& board, int row, int col, bool isWhiteAttacker) {
    int sq = row_col_to_sq(row, col);
    return is_square_attacked_bb(board, sq, isWhiteAttacker);
}

std::vector<Move> get_all_moves(Board& board, bool isWhiteTurn) {
    std::vector<Move> legalMoves;
    legalMoves.reserve(256);
    (void)isWhiteTurn;
    const bool sideToMove = board.isWhiteTurn;

    generate_moves_bb(board, [&](const Move& m) {
        Move tmp = m;
        board.makeMove(tmp);
        int kingRow = 0;
        int kingCol = 0;
        if (!king_square(board, sideToMove, kingRow, kingCol)) {
            board.unmakeMove(tmp);
            return;
        }
        if (!is_square_attacked(board, kingRow, kingCol, !sideToMove)) {
            legalMoves.push_back(tmp);
        }
        board.unmakeMove(tmp);
    });

    return legalMoves;
}

std::vector<Move> get_capture_moves(const Board& board) {
    std::vector<Move> moves;
    moves.reserve(128);

    generate_moves_bb(board, [&](const Move& m) {
        if (m.capturedPiece != 0 || m.isEnPassant) {
            moves.push_back(m);
        }
    });

    return moves;
}
