#include "evaluation.h"
#include <cmath>
#include "board.h"
#include "bitboard.h"
#include <iostream>

// Bit manipulation - using inline functions from types.h
// popcount() and lsb() are defined there
#define set_bit(bitboard, square) ((bitboard) |= (1ULL << (square)))
#define get_bit(bitboard, square) ((bitboard) & (1ULL << (square)))
#define pop_bit(bitboard, square) (get_bit(bitboard, square) ? (bitboard) ^= (1ULL << (square)) : 0)

namespace {

constexpr int OTHER(int side) { return side ^ 1; }

// Vertical flip for A8=0 .. H1=63 indexing.
constexpr int mirror_sq(int sq) { return sq ^ 56; }
}


const int mg_value[6] = { 72, 287, 320, 389, 826, 0 };
const int eg_value[6] = { 117, 341, 347, 619, 1167, 0 };

const int gamephaseInc[6] = { 0, 1, 1, 2, 4, 0 };

// Mobility bonuses and penalties
const int mgKnightMobility[] = { -30, -13, -6, -1, 1, -1, -2, -3, -1 };
const int egKnightMobility[] = { 11, 11, 18, 14, 18, 24, 25, 27, 23 };
const int mgBishopMobility[] = { -34, -27, -19, -17, -10, -3, 3, 6, 7, 9, 12, 17, 15, 44 };
const int egBishopMobility[] = { -43, -21, -14, -4, 9, 22, 26, 32, 40, 38, 37, 36, 40, 24 };
const int mgRookMobility[] = { -25, -16, -12, -9, -10, -3, 1, 8, 12, 18, 23, 28, 28, 32, 34 };
const int egRookMobility[] = { -20, -3, -1, 3, 9, 12, 14, 17, 25, 28, 29, 31, 38, 43, 39 };
const int mgQueenMobility[] = { -27, -23, -31, -28, -25, -23, -18, -20, -18, -16, -15, -15, -14, -13, -11, -11, -8, -11, -8, -6, 4, 7, 7, 11, 34, 89, 90, 139 };
const int egQueenMobility[] = { -106, -126, -50, -32, -26, -19, -13, 7, 14, 17, 27, 33, 40, 46, 51, 57, 60, 71, 76, 76, 74, 76, 79, 79, 60, 40, 37, 27 };

// Pesto tables
const int mg_pawn_table[64] = {
    0, 0, 0, 0, 0, 0, 0, 0,
    60, 90, 65, 93, 78, 64, -2, -38,
    -13, 6, 38, 40, 46, 69, 48, -1,
    -23, 3, 6, 10, 30, 23, 22, -4,
    -31, -5, -1, 12, 14, 13, 11, -14,
    -28, -8, -3, 0, 14, 8, 27, -3,
    -28, -7, -11, -14, 0, 8, 27, -23,
    0, 0, 0, 0, 0, 0, 0, 0
};

const int eg_pawn_table[64] = {
    0, 0, 0, 0, 0, 0, 0, 0,
    184, 175, 178, 124, 121, 136, 182, 193,
    118, 129, 96, 72, 62, 53, 99, 92,
    45, 36, 19, 8, -1, 6, 22, 19,
    18, 18, 2, -5, -6, -2, 8, -2,
    11, 16, 2, 8, 3, 3, 5, -8,
    13, 18, 8, 17, 19, 9, 5, -5,
    0, 0, 0, 0, 0, 0, 0, 0
};

const int mg_knight_table[64] = {
    -154, -130, -67, -36, 0, -56, -105, -102,
    -27, -9, 19, 41, 17, 82, -7, 21,
    -10, 28, 48, 56, 94, 95, 53, 19,
    -10, 7, 33, 55, 36, 60, 20, 26,
    -20, -4, 17, 20, 29, 23, 17, -8,
    -37, -15, 4, 13, 25, 12, 7, -17,
    -44, -34, -18, -3, -3, -10, -27, -28,
    -84, -26, -39, -23, -16, -24, -24, -65
};

const int eg_knight_table[64] = {
    -84, -18, -6, -16, -12, -36, -14, -105,
    -26, -7, -8, -9, -13, -30, -13, -44,
    -12, -5, 12, 12, -4, -8, -16, -23,
    -2, 10, 22, 23, 24, 19, 9, -11,
    -3, 1, 23, 23, 26, 15, 4, -11,
    -19, -5, 0, 16, 14, -4, -12, -17,
    -26, -11, -6, -4, -5, -8, -19, -14,
    -32, -38, -18, -13, -14, -20, -29, -38
};

const int mg_bishop_table[64] = {
    -26, -50, -46, -88, -71, -55, -19, -50,
    -16, 1, -6, -21, 5, 7, 1, -3,
    -5, 16, 13, 30, 20, 51, 37, 25,
    -10, 2, 16, 28, 24, 18, 2, -7,
    -8, -6, 0, 22, 17, 1, -6, 1,
    -2, 7, 9, 7, 10, 11, 9, 12,
    5, 11, 15, -4, 7, 7, 16, -3,
    -4, 15, 2, -9, -1, -15, 1, -5
};

const int eg_bishop_table[64] = {
    0, 10, 3, 16, 9, 0, -7, -3,
    -11, -4, -1, 0, -9, -11, 0, -15,
    11, 2, 4, -5, -1, 2, -3, 5,
    4, 9, 6, 15, 9, 9, 6, 3,
    -2, 9, 13, 11, 10, 9, 5, -9,
    2, 6, 9, 8, 12, 5, 1, -8,
    1, -5, -13, 5, 1, -4, 0, -12,
    -10, 2, -3, 3, 2, 13, -11, -19
};

const int mg_rook_table[64] = {
    20, 3, 4, 6, 22, 46, 43, 59,
    2, -3, 16, 37, 23, 54, 45, 76,
    -17, 4, 0, 2, 30, 39, 84, 57,
    -23, -15, -16, -9, -6, 6, 15, 15,
    -34, -35, -25, -17, -16, -21, -1, -13,
    -33, -31, -20, -15, -8, -8, 16, -4,
    -33, -25, -8, -8, -2, -9, 5, -23,
    -12, -11, 0, 5, 11, -6, 3, -11
};

const int eg_rook_table[64] = {
    13, 21, 30, 25, 18, 8, 8, 5,
    15, 27, 30, 18, 19, 6, 4, -10,
    15, 17, 17, 14, 1, -3, -11, -14,
    17, 14, 22, 16, 3, -2, -3, -6,
    10, 14, 14, 11, 8, 6, -4, -7,
    6, 5, 3, 5, 0, -7, -23, -22,
    0, 3, 3, 4, -4, -7, -16, -10,
    1, 2, 5, 1, -6, -1, -7, -9
};

const int mg_queen_table[64] = {
    -31, -45, -24, 9, 13, 21, 62, 13,
    -10, -33, -31, -39, -34, 12, 1, 48,
    -8, -15, -18, -8, 3, 43, 55, 51,
    -23, -19, -18, -19, -18, -5, 1, 8,
    -17, -22, -19, -12, -12, -12, -6, 2,
    -16, -10, -9, -10, -7, -2, 7, 1,
    -11, -9, 0, 2, 0, 3, 7, 21,
    -11, -13, -3, 1, 2, -21, -3, -7
};

const int eg_queen_table[64] = {
    14, 37, 53, 37, 37, 34, -21, 12,
    -3, 25, 59, 77, 94, 51, 34, 23,
    1, 11, 43, 50, 64, 47, 13, 16,
    9, 17, 29, 49, 62, 56, 48, 38,
    3, 26, 26, 41, 39, 38, 32, 28,
    -5, 8, 16, 14, 19, 18, 7, 3,
    -10, -8, -10, -4, 1, -23, -48, -70,
    -13, -11, -14, 3, -17, -11, -36, -37
};

const int mg_king_table[64] = {
    63, 40, 68, -79, -27, 21, 66, 180,
    -72, -28, -67, 40, -18, -9, 18, -1,
    -96, 11, -58, -75, -34, 37, 12, -23,
    -62, -79, -97, -143, -134, -98, -98, -118,
    -64, -77, -108, -142, -144, -105, -110, -125,
    -28, -23, -75, -90, -87, -85, -45, -47,
    58, 9, -9, -39, -43, -28, 14, 40,
    62, 82, 53, -28, 30, -8, 63, 77
};

const int eg_king_table[64] = {
    -108, -55, -43, 7, -10, -9, -18, -129,
    -9, 21, 33, 15, 37, 47, 39, 6,
    8, 27, 46, 57, 56, 49, 48, 18,
    -3, 33, 52, 64, 64, 58, 50, 23,
    -15, 18, 42, 58, 58, 44, 32, 14,
    -24, 1, 23, 35, 35, 26, 7, -8,
    -46, -16, -2, 9, 12, 3, -16, -37,
    -85, -64, -39, -24, -43, -26, -55, -89
};

// Pointer arrays for easy access
const int* mg_pesto_tables[6] = { mg_pawn_table, mg_knight_table, mg_bishop_table, mg_rook_table, mg_queen_table, mg_king_table };
const int* eg_pesto_tables[6] = { eg_pawn_table, eg_knight_table, eg_bishop_table, eg_rook_table, eg_queen_table, eg_king_table };

namespace {
int mg_table[12][64];
int eg_table[12][64];
bool tables_initialized = false;

int piece_to_table_index(int piece) {
    const int absPiece = piece > 0 ? piece : -piece;
    if (absPiece < 1 || absPiece > 6) return -1;
    return absPiece - 1;
}

void init_tables() {
    for (int p = 0; p < 6; ++p) {
        const int wIdx = p * 2;
        const int bIdx = p * 2 + 1;
        for (int sq = 0; sq < 64; ++sq) {
            const int msq = mirror_sq(sq);
            // PSTs are A8..H1; board squares are A1..H8.
            // White uses mirrored squares, black uses raw squares.
            mg_table[wIdx][sq] = mg_value[p] + mg_pesto_tables[p][msq];
            eg_table[wIdx][sq] = eg_value[p] + eg_pesto_tables[p][msq];
            mg_table[bIdx][sq] = mg_value[p] + mg_pesto_tables[p][sq];
            eg_table[bIdx][sq] = eg_value[p] + eg_pesto_tables[p][sq];
        }
    }
}

void ensure_tables_init() {
    if (!tables_initialized) {
        init_tables();
        tables_initialized = true;
    }
}
}

const int doublePawnPenaltyOpening = -16;
const int doublePawnPenaltyEndgame = -37;

const int mgKingShieldBonus = 16;
const int egKingShieldBonus = -2;

void evaluate_mobility(const Board& board, int pieceType, bool isWhite, Bitboard occupy, int& mgMob, int& egMob) {
    Bitboard myPieces = isWhite ? board.color[WHITE] : board.color[BLACK];
    
    Bitboard pieces = board.piece[pieceType - 1] & myPieces;

    while (pieces) {
        int sq = lsb(pieces);
        pop_bit(pieces, sq);
        
        Bitboard attacks = 0ULL;

        switch (pieceType) {
            case KNIGHT:
                attacks = knight_attacks[sq];
                break;
            case BISHOP:
                attacks = get_bishop_attacks(sq, occupy);
                break;
            case ROOK:
                attacks = get_rook_attacks(sq, occupy);
                break;
            case QUEEN:
                attacks = get_bishop_attacks(sq, occupy) | get_rook_attacks(sq, occupy);
                break;
        }

        Bitboard validMoves = attacks & ~myPieces;
        
        int mobilityCount = popcount(validMoves);

        if (pieceType == KNIGHT) { mgMob += mgKnightMobility[mobilityCount]; egMob += egKnightMobility[mobilityCount]; }
        else if (pieceType == BISHOP) { mgMob += mgBishopMobility[mobilityCount]; egMob += egBishopMobility[mobilityCount]; }
        else if (pieceType == ROOK) { mgMob += mgRookMobility[mobilityCount]; egMob += egRookMobility[mobilityCount]; }
        else if (pieceType == QUEEN) { mgMob += mgQueenMobility[mobilityCount]; egMob += egQueenMobility[mobilityCount]; }
    }
}

int evaluate_board(const Board& board) {
    ensure_tables_init();

    int mg[2] = {0, 0};
    int eg[2] = {0, 0};
    int gamePhase = 0;
    int side2move = board.stm == WHITE ? WHITE : BLACK;

    for (int color = 0; color < 2; color++) {
        for (int p = 0; p < 6; p++) {
            Bitboard bb = board.piece[p] & board.color[color];
            const int tableIdx = p * 2 + (color == BLACK ? 1 : 0);
            while (bb) {
                int sq = lsb(bb);
                bb &= bb - 1;
                mg[color] += mg_table[tableIdx][sq];
                eg[color] += eg_table[tableIdx][sq];
                gamePhase += gamephaseInc[p];
            }
        }
    }

    // Double pawn penalty
    Bitboard file_masks[8] = {
        0x0101010101010101ULL, // File A
        0x0202020202020202ULL, // File B
        0x0404040404040404ULL, // File C
        0x0808080808080808ULL, // File D
        0x1010101010101010ULL, // File E
        0x2020202020202020ULL, // File F
        0x4040404040404040ULL, // File G
        0x8080808080808080ULL  // File H
    };

    for (int color = 0; color < 2; color++) {
        Bitboard pawns = board.piece[PAWN - 1] & board.color[color];
        for (int file = 0; file < 8; file++) {
            int count = popcount(pawns & file_masks[file]);
            if (count > 1) {
                mg[color] += doublePawnPenaltyOpening * (count - 1);
                eg[color] += doublePawnPenaltyEndgame * (count - 1);
            }
        }
    }

    // King shield bonus
    Bitboard whiteShield = king_attacks[lsb(board.piece[KING - 1] & board.color[WHITE])] & board.color[WHITE];
    Bitboard blackShield = king_attacks[lsb(board.piece[KING - 1] & board.color[BLACK])] & board.color[BLACK];
    mg[WHITE] += mgKingShieldBonus * popcount(whiteShield);
    eg[WHITE] += egKingShieldBonus * popcount(whiteShield);
    mg[BLACK] += mgKingShieldBonus * popcount(blackShield);
    eg[BLACK] += egKingShieldBonus * popcount(blackShield);


    /* tapered eval */
    int mgScore = mg[side2move] - mg[OTHER(side2move)];
    int egScore = eg[side2move] - eg[OTHER(side2move)];
    
    int mgPhase = gamePhase;
    if (mgPhase > 24) mgPhase = 24; 
    
    int egPhase = 24 - mgPhase;
    
    // Mobility evaluation
    int mgMob[2] = {0, 0};
    int egMob[2] = {0, 0};
    Bitboard occupy = board.color[WHITE] | board.color[BLACK];
    for (int color = 0; color < 2; color++) {
        bool isWhite = (color == 0);
        for (int pieceType = KNIGHT; pieceType <= QUEEN; pieceType++) {
            evaluate_mobility(board, pieceType, isWhite, occupy, mgMob[color], egMob[color]);
        }
    }
    int sideIdx = (board.stm == WHITE) ? 0 : 1;
    mgScore += mgMob[sideIdx] - mgMob[sideIdx ^ 1];
    egScore += egMob[sideIdx] - egMob[sideIdx ^ 1];

    return (mgScore * mgPhase + egScore * egPhase) / 24;
}

