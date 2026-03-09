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


const int mg_value[6] = { 74, 286, 318, 386, 819, 0 };
const int eg_value[6] = { 116, 338, 345, 613, 1156, 0 };

const int gamephaseInc[6] = { 0, 1, 1, 2, 4, 0 };

// Mobility bonuses and penalties
const int mgKnightMobility[] = { -27, -12, -6, -3, 0, -1, -3, -4, -2 };
const int egKnightMobility[] = { 9, 11, 17, 14, 17, 22, 24, 26, 22 };
const int mgBishopMobility[] = { -34, -26, -19, -17, -10, -3, 3, 6, 7, 9, 12, 15, 13, 42 };
const int egBishopMobility[] = { -42, -22, -14, -5, 8, 21, 25, 31, 39, 37, 35, 35, 39, 23 };
const int mgRookMobility[] = { -28, -18, -13, -9, -9, -2, 2, 9, 13, 19, 24, 29, 29, 34, 36 };
const int egRookMobility[] = { -17, -2, 0, 3, 9, 11, 14, 16, 24, 27, 28, 31, 37, 42, 38 };
const int mgQueenMobility[] = { -21, -23, -31, -28, -25, -22, -18, -20, -18, -15, -15, -15, -14, -13, -11, -11, -8, -11, -8, -6, 4, 7, 7, 12, 34, 89, 86, 124 };
const int egQueenMobility[] = { -77, -110, -46, -29, -26, -22, -16, 4, 10, 14, 24, 30, 37, 43, 48, 54, 57, 68, 73, 73, 71, 73, 75, 76, 57, 37, 37, 33 };

// Pesto tables
const int mg_pawn_table[64] = {
    0, 0, 0, 0, 0, 0, 0, 0,
    58, 87, 62, 89, 74, 62, -2, -35,
    -13, 4, 35, 38, 44, 67, 48, 1,
    -25, 0, 3, 8, 26, 20, 22, -4,
    -33, -7, -4, 9, 12, 9, 9, -15,
    -30, -11, -6, -3, 11, 7, 24, -6,
    -29, -9, -12, -12, 0, 18, 34, -14,
    0, 0, 0, 0, 0, 0, 0, 0
};

const int eg_pawn_table[64] = {
    0, 0, 0, 0, 0, 0, 0, 0,
    182, 173, 176, 123, 120, 135, 180, 190,
    117, 128, 95, 71, 61, 52, 98, 90,
    44, 36, 19, 8, 0, 6, 22, 19,
    18, 18, 2, -4, -5, -1, 8, -2,
    10, 16, 2, 8, 4, 3, 6, -7,
    13, 18, 9, 17, 19, 7, 4, -6,
    0, 0, 0, 0, 0, 0, 0, 0
};

const int mg_knight_table[64] = {
    -149, -129, -68, -38, -1, -57, -105, -103,
    -26, -11, 16, 38, 19, 78, -8, 18,
    -11, 26, 45, 54, 91, 95, 50, 21,
    -11, 6, 30, 53, 34, 57, 18, 25,
    -21, -6, 15, 18, 28, 21, 15, -9,
    -38, -16, 2, 11, 23, 10, 6, -18,
    -44, -34, -19, -2, -2, 0, -17, -18,
    -84, -25, -37, -21, -17, -12, -23, -55
};

const int eg_knight_table[64] = {
    -84, -18, -6, -15, -12, -36, -14, -104,
    -27, -7, -8, -9, -14, -30, -13, -43,
    -12, -5, 11, 12, -4, -9, -15, -24,
    -2, 10, 22, 23, 23, 19, 9, -11,
    -3, 1, 23, 23, 25, 15, 4, -11,
    -19, -5, 0, 17, 14, -4, -11, -17,
    -26, -12, -5, -5, -5, -10, -20, -16,
    -33, -38, -18, -14, -13, -23, -31, -40
};

const int mg_bishop_table[64] = {
    -27, -53, -48, -88, -73, -55, -25, -55,
    -18, -1, -9, -24, 2, 1, -2, -3,
    -6, 14, 10, 28, 16, 49, 33, 25,
    -11, -1, 14, 25, 22, 16, 0, -9,
    -10, -7, -3, 20, 16, -1, -7, 0,
    -3, 5, 7, 6, 8, 9, 7, 10,
    3, 10, 14, -3, 8, 17, 25, 7,
    -6, 15, 2, -8, -2, -5, 14, 3
};

const int eg_bishop_table[64] = {
    0, 10, 3, 16, 9, 0, -6, -3,
    -11, -4, -1, 0, -9, -10, 0, -16,
    11, 1, 4, -5, 0, 2, -3, 5,
    4, 10, 6, 16, 9, 8, 6, 3,
    -2, 9, 13, 11, 10, 9, 5, -10,
    2, 6, 9, 9, 13, 6, 1, -8,
    1, -5, -12, 5, 0, -6, -1, -14,
    -10, 1, -2, 2, 2, 9, -14, -21
};

const int mg_rook_table[64] = {
    15, 0, -1, 1, 17, 37, 36, 54,
    -2, -7, 11, 32, 19, 46, 42, 73,
    -19, 1, -3, -2, 27, 36, 82, 58,
    -25, -17, -19, -13, -9, 2, 14, 16,
    -36, -37, -28, -21, -19, -25, -2, -13,
    -35, -33, -23, -18, -11, -10, 16, -3,
    -35, -27, -10, -9, -4, 0, 13, -17,
    -14, -13, -2, 5, 9, 5, 11, -10
};

const int eg_rook_table[64] = {
    13, 21, 30, 26, 18, 10, 9, 5,
    15, 27, 30, 19, 20, 7, 3, -10,
    15, 17, 18, 15, 2, -3, -12, -15,
    17, 15, 23, 17, 4, -2, -3, -7,
    11, 14, 15, 12, 9, 7, -4, -7,
    6, 5, 4, 6, 1, -6, -23, -22,
    0, 3, 3, 5, -3, -8, -18, -12,
    1, 2, 6, 0, -5, -3, -10, -12
};

const int mg_queen_table[64] = {
    -34, -46, -25, 6, 10, 13, 51, 1,
    -12, -35, -33, -41, -35, 2, -10, 38,
    -8, -15, -19, -10, 0, 41, 51, 52,
    -24, -20, -20, -21, -20, -7, 0, 7,
    -18, -23, -20, -13, -14, -13, -6, 1,
    -17, -11, -10, -11, -8, -2, 6, 2,
    -10, -9, 1, 5, 3, 13, 17, 29,
    -11, -11, -1, 6, 4, -10, 11, 1
};

const int eg_queen_table[64] = {
    15, 36, 53, 38, 36, 37, -17, 18,
    -2, 26, 58, 77, 93, 53, 39, 28,
    1, 11, 44, 50, 63, 46, 14, 14,
    9, 18, 29, 49, 62, 55, 48, 38,
    3, 26, 26, 41, 39, 38, 31, 28,
    -4, 7, 16, 14, 20, 20, 10, 2,
    -11, -8, -12, -7, -1, -26, -49, -71,
    -14, -13, -15, -1, -18, -14, -39, -42
};

const int mg_king_table[64] = {
    57, 38, 64, -78, -30, 19, 64, 166,
    -75, -29, -69, 38, -20, -12, 15, -5,
    -99, 13, -57, -73, -35, 36, 11, -28,
    -64, -78, -94, -137, -129, -93, -92, -119,
    -67, -72, -104, -134, -136, -98, -103, -127,
    -28, -11, -67, -81, -77, -73, -31, -48,
    56, 16, 3, -30, -33, -14, 29, 38,
    51, 80, 59, -36, 23, -11, 61, 58
};

const int eg_king_table[64] = {
    -106, -54, -42, 7, -9, -8, -17, -126,
    -8, 21, 33, 15, 37, 47, 39, 6,
    8, 26, 46, 56, 55, 49, 47, 18,
    -3, 32, 51, 63, 63, 57, 48, 23,
    -14, 17, 41, 56, 56, 43, 31, 14,
    -23, 0, 22, 34, 34, 25, 6, -7,
    -44, -17, -4, 6, 10, 0, -18, -36,
    -80, -61, -42, -23, -43, -26, -54, -81
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
const int doublePawnPenaltyEndgame = -36;

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

