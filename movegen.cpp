#include "board.h"
#include "bitboard.h"
#include "types.h"

#include <vector>

namespace {

inline int get_promo_flag(int promo_piece, bool is_capture) {
    int flag = 8; // Base flag for promotion
    if (promo_piece == BISHOP) flag = FLAG_PROMO_BISHOP;
    else if (promo_piece == ROOK) flag = FLAG_PROMO_ROOK;
    else if (promo_piece == QUEEN) flag = FLAG_PROMO_QUEEN;
    
    if (is_capture) flag += 4;
    return flag;
}

inline void push_move(std::vector<Move>& moves, int fromSq, int toSq, int flag = 0) {
    moves.push_back(static_cast<Move>((flag << 12) | (toSq << 6) | fromSq));
}

inline int piece_on_square_bb(const Board& board, int sq) {
    Bitboard mask = 1ULL << sq;
    if (board.color[WHITE] & mask) {
        for (int i = 0; i < 6; i++) {
            if (board.piece[i] & mask) return i + 1;  // W_PAWN=1 to W_KING=6
        }
        return 0;
    }
    if (board.color[BLACK] & mask) {
        for (int i = 0; i < 6; i++) {
            if (board.piece[i] & mask) return i + 7;  // B_PAWN=7 to B_KING=12
        }
        return 0;
    }
    return 0;
}

inline bool is_king_piece(int piece) {
    return piece_type(piece) == KING;
}

inline Bitboard board_occupancy(const Board& board) {
    return board.color[WHITE] | board.color[BLACK];
}

inline bool is_square_attacked_bb(const Board& board, int sq, bool byWhite) {
    const int us = byWhite ? WHITE : BLACK;
    Bitboard occ = board_occupancy(board);

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

    if (get_bishop_attacks(sq, occ) & (bishops | queens)) return true;
    if (get_rook_attacks(sq, occ) & (rooks | queens)) return true;

    return false;
}

} // namespace

void generate_pawn_moves_bb(const Board& board, std::vector<Move>& moves) {
    const bool whiteToMove = board.stm == WHITE;
    const int us = whiteToMove ? WHITE : BLACK;

    Bitboard own = board.color[us];
    Bitboard opp = board.color[whiteToMove ? BLACK : WHITE];
    Bitboard pawns = board.piece[PAWN - 1] & own;
    Bitboard empty = ~(own | opp);

    while (pawns) {
        int from = lsb(pawns);
        pawns &= pawns - 1;

        int to = whiteToMove ? (from + 8) : (from - 8);
        if (to >= 0 && to < 64) {
            Bitboard toMask = 1ULL << to;
            if (empty & toMask) {
                bool isPromo = whiteToMove ? (from >= 48) : (from <= 15);
                if (isPromo) {
                    for (int promo : {QUEEN, ROOK, BISHOP, KNIGHT}) {
                        push_move(moves, from, to, get_promo_flag(promo, false));
                    }
                } else {
                    push_move(moves, from, to, FLAG_QUIET);
                    
                    bool onStartRank = whiteToMove ? (from >= 8 && from <= 15) : (from >= 48 && from <= 55);
                    if (onStartRank) {
                        int to2 = whiteToMove ? (from + 16) : (from - 16);
                        Bitboard to2Mask = 1ULL << to2;
                        if (empty & to2Mask) {
                            push_move(moves, from, to2, FLAG_DOUBLE_PAWN);
                        }
                    }
                }
            }
        }

        Bitboard attacks = pawn_attacks[us][from] & opp;
        while (attacks) {
            int capSq = lsb(attacks);
            attacks &= attacks - 1;
            int captured = piece_on_square_bb(board, capSq);
            if (is_king_piece(captured)) continue;
            
            bool isPromo = whiteToMove ? (capSq >= 56) : (capSq <= 7);
            if (isPromo) {
                for (int promo : {QUEEN, ROOK, BISHOP, KNIGHT}) {
                    push_move(moves, from, capSq, get_promo_flag(promo, true));
                }
            } else {
                push_move(moves, from, capSq, FLAG_CAPTURE);
            }
        }

        if (board.enPassant != -1) {
            int epRow = whiteToMove ? 2 : 5;
            int epSq = row_col_to_sq(epRow, board.enPassant);
            if (pawn_attacks[us][from] & (1ULL << epSq)) {
                push_move(moves, from, epSq, FLAG_EN_PASSANT);
            }
        }
    }
}

void generate_knight_moves_bb(const Board& board, std::vector<Move>& moves) {
    const bool whiteToMove = board.stm == WHITE;
    const int us = whiteToMove ? WHITE : BLACK;
    const int them = whiteToMove ? BLACK : WHITE;

    Bitboard own = board.color[us];
    Bitboard opp = board.color[them];
    Bitboard knights = board.piece[KNIGHT - 1] & own;

    while (knights) {
        int from = lsb(knights);
        knights &= knights - 1;

        Bitboard targets = knight_attacks[from] & ~own;
        while (targets) {
            int to = lsb(targets);
            targets &= targets - 1;
            
            bool isCapture = (opp & (1ULL << to)) != 0;
            
            if (isCapture) {
                int captured = piece_on_square_bb(board, to);
                if (is_king_piece(captured)) continue; 

                push_move(moves, from, to, FLAG_CAPTURE); 
            } else {
                push_move(moves, from, to, 0); 
            }
        }
    }
}

void generate_bishop_moves_bb(const Board& board, std::vector<Move>& moves) {
    const bool whiteToMove = board.stm == WHITE;
    const int us = whiteToMove ? WHITE : BLACK;
    const int them = whiteToMove ? BLACK : WHITE;

    Bitboard own = board.color[us];
    Bitboard opp = board.color[them];
    Bitboard bishops = board.piece[BISHOP - 1] & own;
    Bitboard occ = board_occupancy(board);

    while (bishops) {
        int from = lsb(bishops);
        bishops &= bishops - 1;

        Bitboard targets = get_bishop_attacks(from, occ) & ~own;
        while (targets) {
            int to = lsb(targets);
            targets &= targets - 1;
            
            bool isCapture = (opp & (1ULL << to)) != 0;
            if (isCapture) {
                int captured = piece_on_square_bb(board, to);
                if (is_king_piece(captured)) continue;
                push_move(moves, from, to, FLAG_CAPTURE);
            } else {
                push_move(moves, from, to, FLAG_QUIET);
            }
        }
    }
}

void generate_rook_moves_bb(const Board& board, std::vector<Move>& moves) {
    const bool whiteToMove = board.stm == WHITE;
    const int us = whiteToMove ? WHITE : BLACK;
    const int them = whiteToMove ? BLACK : WHITE;

    Bitboard own = board.color[us];
    Bitboard opp = board.color[them];
    Bitboard rooks = board.piece[ROOK - 1] & own;
    Bitboard occ = board_occupancy(board);

    while (rooks) {
        int from = lsb(rooks);
        rooks &= rooks - 1;

        Bitboard targets = get_rook_attacks(from, occ) & ~own;
        while (targets) {
            int to = lsb(targets);
            targets &= targets - 1;
            
            bool isCapture = (opp & (1ULL << to)) != 0;
            if (isCapture) {
                int captured = piece_on_square_bb(board, to);
                if (is_king_piece(captured)) continue;
                push_move(moves, from, to, FLAG_CAPTURE);
            } else {
                push_move(moves, from, to, FLAG_QUIET);
            }
        }
    }
}

void generate_queen_moves_bb(const Board& board, std::vector<Move>& moves) {
    const bool whiteToMove = board.stm == WHITE;
    const int us = whiteToMove ? WHITE : BLACK;
    const int them = whiteToMove ? BLACK : WHITE;

    Bitboard own = board.color[us];
    Bitboard opp = board.color[them];
    Bitboard queens = board.piece[QUEEN - 1] & own;
    Bitboard occ = board_occupancy(board);

    while (queens) {
        int from = lsb(queens);
        queens &= queens - 1;

        Bitboard targets = (get_bishop_attacks(from, occ) | get_rook_attacks(from, occ)) & ~own;
        while (targets) {
            int to = lsb(targets);
            targets &= targets - 1;
            
            bool isCapture = (opp & (1ULL << to)) != 0;
            if (isCapture) {
                int captured = piece_on_square_bb(board, to);
                if (is_king_piece(captured)) continue;
                push_move(moves, from, to, FLAG_CAPTURE);
            } else {
                push_move(moves, from, to, FLAG_QUIET);
            }
        }
    }
}

void generate_king_moves_bb(const Board& board, std::vector<Move>& moves) {
    const bool whiteToMove = board.stm == WHITE;
    const int us = whiteToMove ? WHITE : BLACK;
    const int them = whiteToMove ? BLACK : WHITE;

    Bitboard own = board.color[us];
    Bitboard opp = board.color[them];
    Bitboard kings = board.piece[KING - 1] & own;
    if (!kings) return;

    int from = lsb(kings);

    Bitboard targets = king_attacks[from] & ~own;
    while (targets) {
        int to = lsb(targets);
        targets &= targets - 1;
        
        bool isCapture = (opp & (1ULL << to)) != 0;
        if (isCapture) {
            int captured = piece_on_square_bb(board, to);
            if (is_king_piece(captured)) continue;
            push_move(moves, from, to, FLAG_CAPTURE);
        } else {
            push_move(moves, from, to, FLAG_QUIET);
        }
    }

    Bitboard occ = board_occupancy(board);
    const bool opponentIsWhite = !whiteToMove;

    if (whiteToMove && from == 4) {
        if (board.castling & CASTLE_WK) {
            const Bitboard emptyMask = (1ULL << 5) | (1ULL << 6);
            const bool rookPresent = (board.piece[ROOK - 1] & board.color[WHITE]) & (1ULL << 7);
            if ((occ & emptyMask) == 0 &&
                !is_square_attacked_bb(board, 4, opponentIsWhite) &&
                !is_square_attacked_bb(board, 5, opponentIsWhite) &&
                !is_square_attacked_bb(board, 6, opponentIsWhite) &&
                rookPresent) {
                push_move(moves, 4, 6, FLAG_CASTLE_KING);
            }
        }
        if (board.castling & CASTLE_WQ) {
            const Bitboard emptyMask = (1ULL << 1) | (1ULL << 2) | (1ULL << 3);
            const bool rookPresent = (board.piece[ROOK - 1] & board.color[WHITE]) & (1ULL << 0);
            if ((occ & emptyMask) == 0 &&
                !is_square_attacked_bb(board, 4, opponentIsWhite) &&
                !is_square_attacked_bb(board, 3, opponentIsWhite) &&
                !is_square_attacked_bb(board, 2, opponentIsWhite) &&
                rookPresent) {
                push_move(moves, 4, 2, FLAG_CASTLE_QUEEN);
            }
        }
    }

    if (!whiteToMove && from == 60) {
        if (board.castling & CASTLE_BK) {
            const Bitboard emptyMask = (1ULL << 61) | (1ULL << 62);
            const bool rookPresent = (board.piece[ROOK - 1] & board.color[BLACK]) & (1ULL << 63);
            if ((occ & emptyMask) == 0 &&
                !is_square_attacked_bb(board, 60, opponentIsWhite) &&
                !is_square_attacked_bb(board, 61, opponentIsWhite) &&
                !is_square_attacked_bb(board, 62, opponentIsWhite) &&
                rookPresent) {
                push_move(moves, 60, 62, FLAG_CASTLE_KING);
            }
        }
        if (board.castling & CASTLE_BQ) {
            const Bitboard emptyMask = (1ULL << 57) | (1ULL << 58) | (1ULL << 59);
            const bool rookPresent = (board.piece[ROOK - 1] & board.color[BLACK]) & (1ULL << 56);
            if ((occ & emptyMask) == 0 &&
                !is_square_attacked_bb(board, 60, opponentIsWhite) &&
                !is_square_attacked_bb(board, 59, opponentIsWhite) &&
                !is_square_attacked_bb(board, 58, opponentIsWhite) &&
                rookPresent) {
                push_move(moves, 60, 58, FLAG_CASTLE_QUEEN);
            }
        }
    }
}

bool is_square_attacked(const Board& board, int row, int col, bool isWhiteAttacker) {
    int sq = row_col_to_sq(row, col);
    return is_square_attacked_bb(board, sq, isWhiteAttacker);
}

std::vector<Move> get_all_moves(Board& board, bool isWhiteTurn) {
    std::vector<Move> pseudoMoves;
    std::vector<Move> legalMoves;
    pseudoMoves.reserve(256);
    (void)isWhiteTurn;
    const bool sideToMove = board.stm == WHITE;

    generate_pawn_moves_bb(board, pseudoMoves);
    generate_knight_moves_bb(board, pseudoMoves);
    generate_bishop_moves_bb(board, pseudoMoves);
    generate_rook_moves_bb(board, pseudoMoves);
    generate_queen_moves_bb(board, pseudoMoves);
    generate_king_moves_bb(board, pseudoMoves);

    for (auto& m : pseudoMoves) {
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

    for (auto& m : pseudoMoves) {
        if (is_capture(m)) {
            moves.push_back(m);
        }
    }

    return moves;
}
