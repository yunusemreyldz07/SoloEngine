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


const int mg_value[6] = { 79, 288, 323, 394, 781, -34 };
const int eg_value[6] = { 142, 355, 379, 637, 1191, 6 };

const int gamephaseInc[6] = { 0, 1, 1, 2, 4, 0 };

// Mobility bonuses and penalties
// Can go to 8 squares max
// 0 squares = -20
// 4 squares = 0
// 8 squares = +15
const int KnightMobility[9] = { -20, -10, -5, -2, 0, 5, 10, 12, 15 };

// Can go to 13 squares max
// Same logic here, hopefully this will prevent bad BISHOP placements lol
// 0 squares = -20
// 6 squares = 0
// 13 squares = +20
const int BishopMobility[14] = { -20, -10, -5, -2, 0, 2, 4, 6, 8, 10, 12, 15, 18, 20 };

// and the rest...
const int RookMobility[15] = { -10, -5, -2, 0, 2, 4, 6, 8, 10, 12, 14, 16, 20, 25, 30 };

// QUEEN bonuses are not that much according to other pieces, it is because QUEEN already can go to many squares and this might cause our engine to get its QUEEN out too early
const int QueenMobility[28] = { -5, -3, -2, -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 15, 15, 15, 15, 15, 15, 15, 15 };

// Pesto tables
const int mg_pawn_table[64] = {
      3,    3,    3,    3,    3,    3,    3,    3,
     68,   77,   59,   91,   70,   55,  -15,  -26,
    -14,  -14,   26,   34,   36,   57,   29,   -1,
    -33,  -23,   -8,   -5,   14,    7,   -1,  -11,
    -44,  -32,  -21,   -4,   -5,  -12,  -16,  -24,
    -46,  -36,  -23,  -22,   -9,  -17,   -1,  -18,
    -46,  -37,  -28,  -38,  -19,   -4,    6,  -26,
      3,    3,    3,    3,    3,    3,    3,    3
};

const int eg_pawn_table[64] = {
    -48,  -48,  -48,  -48,  -48,  -48,  -48,  -48,
    143,  136,  137,   86,   83,   96,  140,  151,
     84,   94,   61,   37,   28,   17,   63,   58,
     15,    6,  -12,  -23,  -31,  -26,  -10,  -12,
    -11,  -13,  -27,  -34,  -35,  -31,  -23,  -31,
    -17,  -15,  -27,  -20,  -25,  -25,  -26,  -35,
    -13,  -12,  -19,  -18,  -12,  -22,  -28,  -34,
    -48,  -48,  -48,  -48,  -48,  -48,  -48,  -48
};

const int mg_knight_table[64] = {
   -145, -110,  -48,  -19,   15,  -42,  -97,  -92,
    -13,    1,   29,   48,   31,   89,    1,   27,
      3,   37,   53,   65,  101,  103,   60,   30,
     -1,   12,   37,   57,   40,   64,   23,   33,
    -13,    2,   16,   17,   26,   21,   20,   -3,
    -32,   -9,    3,    7,   18,    8,   12,  -16,
    -43,  -33,  -16,   -5,   -4,   -2,  -14,  -18,
    -85,  -35,  -46,  -32,  -28,  -16,  -32,  -57
};

const int eg_knight_table[64] = {
    -76,  -14,   -1,   -9,   -6,  -28,   -8,  -96,
    -23,   -1,    5,    5,   -1,  -17,   -6,  -38,
     -6,    9,   25,   25,    9,    4,   -2,  -17,
      5,   24,   35,   38,   38,   32,   23,   -4,
      4,   14,   38,   37,   40,   31,   17,   -3,
    -10,    9,   17,   31,   30,   14,    3,   -8,
    -19,   -4,    7,    9,    7,    4,  -12,   -8,
    -27,  -41,  -10,   -7,   -6,  -16,  -33,  -34
};

const int mg_bishop_table[64] = {
    -29,  -45,  -35,  -74,  -64,  -48,  -17,  -58,
    -11,    8,    6,  -10,   18,   18,    4,    0,
      2,   23,   23,   46,   34,   63,   42,   30,
     -8,    6,   26,   36,   33,   29,    6,   -7,
    -14,   -2,    5,   24,   22,    6,   -1,   -6,
     -4,    4,    2,    6,    7,    2,    5,    8,
     -1,   -1,   11,  -11,   -3,    8,   15,    2,
    -24,   -2,  -20,  -27,  -23,  -24,    0,  -13
};

const int eg_bishop_table[64] = {
    -10,    2,   -2,   11,    6,   -3,  -10,  -12,
    -23,   -4,    0,    4,   -6,   -8,    0,  -24,
      6,    0,   11,    0,    5,    6,   -1,   -1,
      0,   17,   12,   24,   17,   15,   14,    2,
     -2,   15,   22,   19,   18,   17,   11,  -13,
     -2,    7,   15,   16,   20,   14,   -1,  -13,
     -8,   -8,   -9,    5,    7,   -5,   -3,  -27,
    -26,   -7,  -27,   -5,   -9,   -7,  -22,  -38
};

const int mg_rook_table[64] = {
     27,   19,   26,   30,   47,   62,   45,   65,
     11,    7,   26,   46,   33,   61,   47,   79,
     -6,   11,   13,   15,   43,   46,   84,   59,
    -23,  -12,  -10,   -2,    4,    5,   14,   15,
    -44,  -42,  -31,  -19,  -19,  -33,  -10,  -19,
    -51,  -41,  -33,  -33,  -28,  -30,    3,  -18,
    -54,  -42,  -27,  -30,  -26,  -24,   -7,  -38,
    -35,  -33,  -24,  -19,  -15,  -25,  -10,  -33
};

const int eg_rook_table[64] = {
      9,   16,   24,   21,   12,    1,    4,   -1,
      8,   20,   24,   16,   15,    0,   -3,  -16,
      8,   11,   12,   10,   -3,   -9,  -17,  -20,
     10,   10,   18,   14,   -1,   -6,   -9,  -14,
      4,    9,   10,    9,    4,    3,  -10,  -14,
     -1,   -1,   -1,    2,   -2,   -9,  -28,  -28,
     -7,   -2,   -2,    0,   -9,  -12,  -22,  -17,
    -11,   -1,    6,    4,   -3,   -8,  -10,  -20
};

const int mg_queen_table[64] = {
    -38,  -27,    0,   34,   35,   40,   60,    2,
      2,  -26,  -16,  -24,  -17,   20,   -3,   46,
      2,   -1,   -5,   13,   18,   58,   62,   58,
    -16,  -11,   -7,   -8,   -6,    6,    7,   12,
    -13,  -15,  -17,   -8,  -10,  -10,    1,    4,
    -15,   -8,  -13,  -14,  -11,   -4,    8,    2,
    -18,  -12,   -2,   -3,   -4,    5,   10,   20,
    -20,  -29,  -22,   -8,  -16,  -29,   -6,  -14
};

const int eg_queen_table[64] = {
      3,   15,   31,   17,   14,    6,  -36,   -2,
    -33,   11,   43,   63,   77,   35,   22,   -6,
    -21,   -3,   36,   37,   51,   31,   -6,  -17,
    -11,   11,   23,   49,   60,   45,   32,   11,
    -16,   12,   24,   42,   40,   31,   10,   -1,
    -28,  -11,   12,   10,   13,    5,  -18,  -30,
    -34,  -29,  -31,  -20,  -18,  -44,  -73, -103,
    -39,  -31,  -28,  -37,  -33,  -34,  -64,  -64
};

const int mg_king_table[64] = {
     67,   41,   69,  -68,  -14,   41,   86,  183,
    -47,  -12,  -58,   47,   -4,    6,   40,   23,
    -70,   30,  -41,  -56,  -15,   60,   41,    3,
    -35,  -48,  -66, -110,  -96,  -60,  -58,  -82,
    -33,  -43,  -72,  -99,  -99,  -62,  -68,  -90,
      6,   24,  -32,  -45,  -41,  -37,    7,  -10,
     93,   54,   40,    6,    4,   23,   68,   78,
     92,  112,   86,  -11,   51,   13,   93,   95
};

const int eg_king_table[64] = {
   -104,  -54,  -44,    5,  -13,  -11,  -19, -122,
    -12,   17,   30,   12,   33,   43,   34,    3,
      3,   22,   41,   51,   50,   43,   42,   14,
     -7,   27,   44,   57,   56,   51,   43,   18,
    -18,   12,   35,   50,   50,   37,   26,    9,
    -28,   -6,   15,   28,   28,   19,    0,  -12,
    -50,  -23,  -11,    0,    4,   -6,  -24,  -41,
    -84,  -65,  -47,  -28,  -53,  -29,  -56,  -82
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

const int doublePawnPenaltyOpening = 1;
const int doublePawnPenaltyEndgame = -30;

const int isolatedPawnPenaltyOpening = -16;
const int isolatedPawnPenaltyEndgame = -3;

int evaluate_mobility(const Board& board, int pieceType, bool isWhite, Bitboard occupy) {
    Bitboard myPieces = isWhite ? board.color[WHITE] : board.color[BLACK];
    
    Bitboard pieces = board.piece[pieceType - 1] & myPieces;
    int totalMobility = 0;

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

        if (pieceType == KNIGHT) totalMobility += KnightMobility[mobilityCount];
        else if (pieceType == BISHOP) totalMobility += BishopMobility[mobilityCount];
        else if (pieceType == ROOK) totalMobility += RookMobility[mobilityCount];
        else if (pieceType == QUEEN) totalMobility += QueenMobility[mobilityCount];
    }
    
    return totalMobility;
}

void evaluate_pawn_penalties(const Board& board, int mg[2], int eg[2]) {
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

    const Bitboard adjacent_file_masks[8] = {
        0x0202020202020202ULL, // File A's neighbor: Only File B
        file_masks[0] | file_masks[2], // File B's neighbors: File A | File C
        file_masks[1] | file_masks[3], // File C's neighbors: File B | File D
        file_masks[2] | file_masks[4], // File D's neighbors: File C | File E
        file_masks[3] | file_masks[5], // File E's neighbors: File D | File F
        file_masks[4] | file_masks[6], // File F's neighbors: File E | File G
        file_masks[5] | file_masks[7], // File G's neighbors: File F | File H
        0x4040404040404040ULL  // File H's neighbor: Only File G
    };

    for (int color = 0; color < 2; color++) {
        Bitboard pawns = board.piece[PAWN - 1] & board.color[color];
        for (int file = 0; file < 8; file++) {
            int count = popcount(pawns & file_masks[file]);
            if (count > 1) {
                mg[color] += doublePawnPenaltyOpening * (count - 1);
                eg[color] += doublePawnPenaltyEndgame * (count - 1);
            }

            if (count > 0) {
                Bitboard adjacentFiles = adjacent_file_masks[file];
                if ((pawns & adjacentFiles) == 0) {
                    mg[color] += isolatedPawnPenaltyOpening * count;
                    eg[color] += isolatedPawnPenaltyEndgame * count;
                }
            }

        }
    }
}

int evaluate_board(const Board& board, bool include_pawn_penalties, bool include_psqt) {
    ensure_tables_init();

    int mg[2] = {0, 0};
    int eg[2] = {0, 0};
    int gamePhase = 0;
    int side2move = board.stm == WHITE ? WHITE : BLACK;

    if (include_psqt) {
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
    } else {
        // Still need gamePhase for tapered eval even without PST
        for (int color = 0; color < 2; color++) {
            for (int p = 0; p < 6; p++) {
                Bitboard bb = board.piece[p] & board.color[color];
                while (bb) {
                    bb &= bb - 1;
                    gamePhase += gamephaseInc[p];
                }
            }
        }
    }

    // Pawn structure penalties
    if (include_pawn_penalties) {
        evaluate_pawn_penalties(board, mg, eg);
    }

    /* tapered eval */
    int mgScore = mg[side2move] - mg[OTHER(side2move)];
    int egScore = eg[side2move] - eg[OTHER(side2move)];
    
    int mgPhase = gamePhase;
    if (mgPhase > 24) mgPhase = 24; 
    
    int egPhase = 24 - mgPhase;
    
    int staticEval = (mgScore * mgPhase + egScore * egPhase) / 24;

    // Mobility evaluation
    int mobilityScore = 0;
    Bitboard occupy = board.color[WHITE] | board.color[BLACK];
    for (int pieceType = KNIGHT; pieceType <= QUEEN; pieceType++) {
        mobilityScore += evaluate_mobility(board, pieceType, board.stm == WHITE, occupy);
        mobilityScore -= evaluate_mobility(board, pieceType, !(board.stm == WHITE), occupy);
    }

    return (staticEval + mobilityScore);
}

