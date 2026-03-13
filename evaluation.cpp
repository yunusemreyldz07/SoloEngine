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


const int mg_value[6] = { 70, 290, 324, 394, 838, 0 };
const int eg_value[6] = { 87, 352, 360, 642, 1210, 0 };

const int gamephaseInc[6] = { 0, 1, 1, 2, 4, 0 };

// Mobility bonuses and penalties
const int mgKnightMobility[] = { -28, -12, -4, 0, 2, 1, -1, -1, 0 };
const int egKnightMobility[] = { 12, 14, 20, 18, 23, 28, 30, 31, 28 };
const int mgBishopMobility[] = { -33, -26, -18, -16, -9, -2, 4, 7, 7, 10, 12, 16, 16, 40 };
const int egBishopMobility[] = { -38, -20, -12, -1, 12, 25, 29, 36, 43, 42, 40, 40, 44, 31 };
const int mgRookMobility[] = { -25, -16, -12, -8, -9, -2, 2, 9, 13, 18, 23, 27, 28, 33, 33 };
const int egRookMobility[] = { -20, -2, 1, 5, 12, 15, 17, 19, 27, 31, 33, 37, 43, 46, 44 };
const int mgQueenMobility[] = { -26, -21, -30, -27, -24, -22, -17, -19, -17, -15, -14, -14, -13, -12, -11, -11, -8, -11, -8, -6, 3, 7, 7, 12, 33, 91, 95, 132 };
const int egQueenMobility[] = { -123, -130, -50, -32, -25, -18, -13, 8, 16, 20, 30, 36, 44, 50, 55, 61, 65, 76, 82, 82, 81, 83, 85, 86, 67, 47, 39, 39 };

// Pesto tables
const int mg_pawn_table[64] = {
    0, 0, 0, 0, 0, 0, 0, 0,
    51, 63, 45, 78, 59, 38, -35, -50,
    -3, 2, 34, 39, 46, 81, 63, 30,
    -16, -3, 8, 13, 34, 26, 19, 9,
    -25, -11, 4, 19, 19, 17, 5, -5,
    -23, -15, 1, 5, 17, 13, 22, 6,
    -23, -14, -8, -10, 2, 12, 21, -15,
    0, 0, 0, 0, 0, 0, 0, 0
};

const int eg_pawn_table[64] = {
    0, 0, 0, 0, 0, 0, 0, 0,
    174, 159, 159, 95, 93, 117, 165, 182,
    45, 45, 32, 42, 20, 21, 47, 37,
    37, 30, 28, 14, 13, 16, 20, 15,
    22, 23, 21, 18, 17, 17, 12, 6,
    18, 19, 20, 25, 25, 20, 7, 2,
    21, 20, 26, 33, 39, 26, 7, 4,
    0, 0, 0, 0, 0, 0, 0, 0
};

const int mg_knight_table[64] = {
    -158, -132, -69, -37, -1, -60, -109, -107,
    -29, -12, 19, 43, 17, 82, -11, 17,
    -10, 30, 48, 58, 96, 100, 55, 20,
    -8, 8, 35, 58, 38, 62, 21, 27,
    -20, -3, 18, 21, 31, 24, 19, -7,
    -37, -14, 5, 14, 27, 13, 8, -17,
    -44, -35, -17, -3, -2, -9, -26, -28,
    -84, -26, -39, -22, -16, -24, -24, -66
};

const int eg_knight_table[64] = {
    -85, -20, -5, -16, -13, -34, -17, -105,
    -21, -7, -8, -9, -12, -30, -11, -39,
    -14, -6, 12, 13, -3, -11, -14, -23,
    -1, 11, 23, 24, 24, 21, 10, -11,
    -1, 4, 24, 25, 27, 15, 3, -9,
    -18, -3, 2, 18, 14, -4, -12, -16,
    -22, -10, -5, -4, -5, -8, -19, -13,
    -29, -35, -15, -13, -14, -20, -29, -33
};

const int mg_bishop_table[64] = {
    -29, -54, -50, -90, -71, -57, -21, -52,
    -16, -3, -7, -22, 6, 6, -3, -5,
    -4, 17, 14, 30, 21, 52, 36, 25,
    -10, 3, 16, 30, 25, 19, 2, -8,
    -8, -5, 1, 23, 19, 2, -5, 1,
    -3, 8, 10, 8, 11, 11, 10, 12,
    5, 11, 16, -4, 7, 8, 17, -3,
    -4, 16, 2, -8, -1, -14, 0, -4
};

const int eg_bishop_table[64] = {
    2, 13, 4, 16, 9, 0, -2, -2,
    -7, -2, 0, 1, -10, -9, 1, -12,
    10, 1, 4, -4, -1, 4, -2, 6,
    5, 9, 6, 16, 11, 9, 7, 4,
    2, 9, 14, 14, 10, 9, 5, -8,
    3, 8, 10, 9, 13, 6, 1, -7,
    4, -5, -12, 5, 1, -4, 0, -10,
    -9, 2, -2, 4, 2, 13, -10, -21
};

const int mg_rook_table[64] = {
    19, 2, 5, 5, 22, 44, 39, 59,
    3, -5, 16, 37, 23, 55, 46, 76,
    -16, 6, 0, 4, 33, 40, 89, 59,
    -23, -16, -14, -7, -5, 5, 16, 16,
    -35, -35, -26, -17, -16, -22, 0, -14,
    -34, -31, -21, -15, -7, -8, 17, -5,
    -34, -26, -8, -8, -2, -9, 6, -25,
    -12, -11, 0, 6, 12, -6, 5, -12
};

const int eg_rook_table[64] = {
    14, 23, 31, 26, 18, 11, 12, 6,
    17, 29, 31, 20, 21, 7, 5, -6,
    16, 15, 17, 13, 1, -5, -14, -15,
    19, 16, 21, 16, 3, -2, -2, -7,
    15, 16, 15, 12, 9, 7, -3, -3,
    9, 7, 5, 6, 0, -6, -23, -18,
    3, 5, 3, 4, -4, -7, -17, -9,
    2, 2, 5, 1, -6, 0, -8, -8
};

const int mg_queen_table[64] = {
    -33, -44, -25, 9, 9, 26, 63, 10,
    -8, -37, -31, -42, -35, 11, -3, 47,
    -7, -14, -19, -7, 4, 43, 55, 51,
    -23, -18, -17, -18, -17, -5, 1, 7,
    -17, -22, -19, -12, -12, -12, -5, 2,
    -15, -9, -9, -10, -6, -2, 7, 2,
    -10, -9, 1, 2, 1, 4, 8, 22,
    -11, -12, -3, 2, 3, -21, -2, -7
};

const int eg_queen_table[64] = {
    16, 36, 54, 39, 41, 33, -21, 17,
    0, 29, 60, 83, 98, 53, 40, 29,
    1, 12, 43, 50, 66, 48, 15, 20,
    13, 20, 31, 52, 64, 58, 53, 43,
    6, 27, 27, 44, 42, 41, 33, 33,
    -6, 8, 18, 16, 22, 20, 8, 4,
    -9, -8, -11, -3, 1, -24, -51, -72,
    -14, -12, -14, 3, -17, -14, -37, -38
};

const int mg_king_table[64] = {
    62, 46, 59, -73, -36, 29, 86, 207,
    -81, -32, -67, 32, -17, -10, 37, 15,
    -98, 15, -56, -75, -34, 46, 23, -16,
    -63, -78, -99, -145, -137, -98, -94, -111,
    -76, -79, -115, -150, -149, -107, -113, -133,
    -29, -26, -80, -95, -91, -90, -48, -51,
    61, 9, -12, -42, -47, -31, 12, 39,
    64, 81, 52, -30, 29, -10, 62, 78
};

const int eg_king_table[64] = {
    -112, -57, -41, 7, -7, -10, -20, -134,
    -9, 20, 33, 18, 37, 49, 34, 3,
    4, 24, 45, 57, 57, 49, 45, 13,
    -6, 30, 52, 65, 66, 60, 48, 18,
    -13, 18, 44, 61, 60, 45, 31, 14,
    -26, 2, 26, 38, 38, 29, 9, -7,
    -45, -14, 0, 10, 14, 5, -14, -37,
    -87, -64, -38, -24, -43, -25, -55, -91
};

// Pointer arrays for easy access
const int* mg_pesto_tables[6] = { mg_pawn_table, mg_knight_table, mg_bishop_table, mg_rook_table, mg_queen_table, mg_king_table };
const int* eg_pesto_tables[6] = { eg_pawn_table, eg_knight_table, eg_bishop_table, eg_rook_table, eg_queen_table, eg_king_table };

namespace {
int mg_table[12][64];
int eg_table[12][64];

const Bitboard file_masks[8] = {
    0x0101010101010101ULL, 0x0202020202020202ULL,
    0x0404040404040404ULL, 0x0808080808080808ULL,
    0x1010101010101010ULL, 0x2020202020202020ULL,
    0x4040404040404040ULL, 0x8080808080808080ULL
};

const Bitboard adjacent_file_masks[8] = {
    file_masks[1],
    file_masks[0] | file_masks[2],
    file_masks[1] | file_masks[3],
    file_masks[2] | file_masks[4],
    file_masks[3] | file_masks[5],
    file_masks[4] | file_masks[6],
    file_masks[5] | file_masks[7],
    file_masks[6]
};

Bitboard white_passed_mask[64];
Bitboard black_passed_mask[64];

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
            mg_table[wIdx][sq] = mg_value[p] + mg_pesto_tables[p][msq];
            eg_table[wIdx][sq] = eg_value[p] + eg_pesto_tables[p][msq];
            mg_table[bIdx][sq] = mg_value[p] + mg_pesto_tables[p][sq];
            eg_table[bIdx][sq] = eg_value[p] + eg_pesto_tables[p][sq];
        }
    }

    for (int sq = 0; sq < 64; ++sq) {
        int file = sq & 7;
        int rank = sq >> 3;
        Bitboard fileMask = file_masks[file] | adjacent_file_masks[file];

        // White: ranks above this square (toward rank 8)
        if (rank >= 7)
            white_passed_mask[sq] = 0;
        else
            white_passed_mask[sq] = fileMask & ~((1ULL << ((rank + 1) * 8)) - 1);

        // Black: ranks below this square (toward rank 1)
        if (rank == 0)
            black_passed_mask[sq] = 0;
        else
            black_passed_mask[sq] = fileMask & ((1ULL << (rank * 8)) - 1);
    }
}

void ensure_tables_init() {
    if (!tables_initialized) {
        init_tables();
        tables_initialized = true;
    }
}
}

const int doublePawnPenaltyOpening = -7;
const int doublePawnPenaltyEndgame = -18;

const int mgKingShieldBonus = 16;
const int egKingShieldBonus = -2;

const int mgIsolatedPawnPenalty = -15;
const int egIsolatedPawnPenalty = -14;

const int aggressivenessWeight = 15;

const int passed_pawn_mg_table[64] = {
    0, 0, 0, 0, 0, 0, 0, 0,
    15, 18, 20, 20, 19, 17, 19, 14,
    28, 41, 22, 10, 11, 6, -23, -42,
    25, 22, 20, 18, 1, 12, -8, -10,
    10, 2, -16, -7, -15, -7, -12, -2,
    3, -9, -23, -20, -19, -12, -13, 10,
    -2, 1, -16, -18, -6, -1, 15, 5,
    0, 0, 0, 0, 0, 0, 0, 0
};

const int passed_pawn_eg_table[64] = {
    0, 0, 0, 0, 0, 0, 0, 0,
    68, 66, 68, 73, 74, 69, 65, 66,
    157, 153, 120, 70, 86, 106, 125, 152,
    87, 81, 52, 44, 43, 52, 80, 87,
    55, 46, 31, 20, 24, 30, 55, 52,
    16, 21, 13, 6, 6, 8, 35, 17,
    15, 16, 13, 8, -8, 1, 14, 17,
    0, 0, 0, 0, 0, 0, 0, 0
};

void evaluate_mobility(const Board& board, int pieceType, bool isWhite, Bitboard occupy, int& mgMob, int& egMob, int& kingAttacks, Bitboard enemyKingZone) {
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

        kingAttacks += popcount(validMoves & enemyKingZone);

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

    // Pawn structure evaluation
    for (int color = 0; color < 2; color++) {
        Bitboard pawns = board.piece[PAWN - 1] & board.color[color];
        Bitboard enemyPawns = board.piece[PAWN - 1] & board.color[OTHER(color)];
        for (int file = 0; file < 8; file++) {
            int count = popcount(pawns & file_masks[file]);
            if (count > 1) {
                mg[color] += doublePawnPenaltyOpening * (count - 1);
                eg[color] += doublePawnPenaltyEndgame * (count - 1);
            }

            if (count > 0) {
                if ((pawns & adjacent_file_masks[file]) == 0) {
                    mg[color] += mgIsolatedPawnPenalty * count;
                    eg[color] += egIsolatedPawnPenalty * count;
                }
            }
        }

        // Passed pawn bonus
        const Bitboard* passedMasks = (color == WHITE) ? white_passed_mask : black_passed_mask;
        Bitboard pawnsCopy = pawns;
        while (pawnsCopy) {
            int sq = lsb(pawnsCopy);
            pawnsCopy &= pawnsCopy - 1;

            if ((enemyPawns & passedMasks[sq]) == 0) {
                int tableIdx = (color == WHITE) ? mirror_sq(sq) : sq;
                mg[color] += passed_pawn_mg_table[tableIdx];
                eg[color] += passed_pawn_eg_table[tableIdx];
            }
        }
    }

    // King shield bonus
    Bitboard whiteShield = king_attacks[lsb(board.piece[KING - 1] & board.color[WHITE])] & board.color[WHITE];
    Bitboard blackShield = king_attacks[lsb(board.piece[KING - 1] & board.color[BLACK])] & board.color[BLACK];
    
    Bitboard whiteKingZone = king_attacks[lsb(board.piece[KING - 1] & board.color[WHITE])];
    Bitboard blackKingZone = king_attacks[lsb(board.piece[KING - 1] & board.color[BLACK])];

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
    int kingAttacks[2] = {0, 0};

    Bitboard occupy = board.color[WHITE] | board.color[BLACK];
    for (int color = 0; color < 2; color++) {
        bool isWhite = (color == 0);
        for (int pieceType = KNIGHT; pieceType <= QUEEN; pieceType++) {
            evaluate_mobility(board, pieceType, isWhite, occupy, mgMob[color], egMob[color], kingAttacks[color], (color == WHITE) ? blackKingZone : whiteKingZone);
        }
    }
    int sideIdx = (board.stm == WHITE) ? 0 : 1;
    mgScore += mgMob[sideIdx] - mgMob[sideIdx ^ 1];
    egScore += egMob[sideIdx] - egMob[sideIdx ^ 1];
    
    int whiteAttackScore = (kingAttacks[WHITE] * kingAttacks[WHITE] * aggressivenessWeight) / 10;
    int blackAttackScore = (kingAttacks[BLACK] * kingAttacks[BLACK] * aggressivenessWeight) / 10;

    int attackDiff = whiteAttackScore - blackAttackScore;

    if (side2move == BLACK) {
        attackDiff = -attackDiff;
    }

    mgScore += attackDiff;

    return (mgScore * mgPhase + egScore * egPhase) / 24;
}

