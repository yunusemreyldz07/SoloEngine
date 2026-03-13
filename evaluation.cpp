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


const int mg_value[6] = { 71, 292, 326, 398, 842, 0 };
const int eg_value[6] = { 87, 351, 359, 641, 1210, 0 };

const int gamephaseInc[6] = { 0, 1, 1, 2, 4, 0 };

// Mobility bonuses and penalties
const int mgKnightMobility[] = { -27, -11, -3, 1, 3, 2, 0, 0, 1 };
const int egKnightMobility[] = { 9, 14, 20, 18, 23, 28, 29, 31, 28 };
const int mgBishopMobility[] = { -32, -25, -17, -14, -7, -1, 5, 8, 8, 10, 13, 16, 15, 36 };
const int egBishopMobility[] = { -37, -20, -12, -1, 12, 25, 28, 35, 43, 41, 40, 40, 44, 32 };
const int mgRookMobility[] = { -24, -14, -10, -7, -8, -1, 3, 10, 13, 18, 22, 25, 25, 28, 25 };
const int egRookMobility[] = { -20, -2, 1, 5, 12, 14, 17, 19, 26, 30, 33, 37, 42, 46, 45 };
const int mgQueenMobility[] = { -24, -21, -30, -26, -23, -21, -16, -18, -16, -13, -13, -13, -12, -11, -10, -10, -7, -10, -7, -6, 3, 7, 7, 11, 30, 88, 96, 117 };
const int egQueenMobility[] = { -129, -125, -47, -29, -25, -16, -11, 10, 18, 21, 31, 38, 45, 51, 56, 61, 65, 75, 81, 81, 79, 81, 82, 83, 65, 44, 34, 40 };

// Pesto tables
const int mg_pawn_table[64] = {
    0, 0, 0, 0, 0, 0, 0, 0,
    53, 64, 47, 80, 61, 41, -33, -47,
    -2, 3, 34, 39, 47, 82, 64, 30,
    -16, -3, 8, 13, 34, 26, 19, 8,
    -25, -11, 4, 19, 19, 16, 5, -6,
    -23, -14, 1, 5, 17, 12, 21, 4,
    -23, -14, -8, -10, 2, 12, 22, -15,
    0, 0, 0, 0, 0, 0, 0, 0
};

const int eg_pawn_table[64] = {
    0, 0, 0, 0, 0, 0, 0, 0,
    173, 159, 158, 95, 93, 116, 164, 181,
    45, 45, 33, 42, 20, 21, 47, 37,
    37, 30, 28, 14, 13, 16, 20, 15,
    22, 23, 21, 18, 16, 17, 12, 6,
    18, 19, 20, 25, 25, 20, 7, 2,
    21, 20, 26, 33, 39, 26, 7, 4,
    0, 0, 0, 0, 0, 0, 0, 0
};

const int mg_knight_table[64] = {
    -155, -132, -71, -36, -6, -63, -117, -106,
    -30, -12, 16, 41, 12, 80, -10, 14,
    -10, 30, 48, 57, 89, 94, 49, 15,
    -6, 9, 35, 58, 37, 61, 16, 27,
    -18, -1, 19, 22, 32, 25, 19, -6,
    -35, -12, 7, 16, 29, 14, 10, -15,
    -42, -32, -15, -1, -1, -7, -24, -26,
    -80, -24, -37, -20, -14, -20, -22, -61
};

const int eg_knight_table[64] = {
    -86, -20, -5, -17, -12, -34, -15, -105,
    -21, -7, -7, -9, -12, -30, -12, -39,
    -14, -6, 12, 12, -2, -10, -13, -22,
    -2, 11, 23, 23, 24, 21, 9, -11,
    -2, 3, 23, 24, 26, 14, 2, -10,
    -18, -3, 2, 17, 14, -3, -11, -16,
    -22, -10, -5, -4, -5, -8, -19, -13,
    -29, -35, -16, -14, -14, -21, -28, -33
};

const int mg_bishop_table[64] = {
    -28, -54, -51, -90, -76, -59, -23, -56,
    -15, -1, -6, -22, 5, 3, -3, -5,
    -3, 18, 14, 30, 18, 46, 30, 22,
    -10, 4, 16, 30, 25, 19, 3, -8,
    -7, -5, 1, 24, 20, 2, -4, 2,
    -2, 9, 10, 9, 11, 12, 11, 13,
    6, 12, 17, -2, 9, 10, 19, 0,
    -4, 16, 3, -8, 0, -12, 2, 0
};

const int eg_bishop_table[64] = {
    2, 13, 4, 16, 10, 1, -1, -1,
    -7, -2, 0, 1, -10, -8, 1, -13,
    10, 1, 4, -4, 0, 5, -1, 6,
    5, 9, 6, 16, 10, 9, 6, 4,
    1, 9, 13, 13, 10, 9, 5, -9,
    3, 8, 10, 9, 14, 6, 1, -7,
    3, -5, -12, 5, 1, -4, 0, -11,
    -9, 1, -2, 4, 2, 12, -10, -21
};

const int mg_rook_table[64] = {
    13, -4, -1, -11, 2, 23, 20, 39,
    4, -4, 18, 36, 22, 51, 41, 64,
    -13, 10, 4, 6, 33, 35, 86, 49,
    -20, -11, -11, -3, -1, 6, 15, 15,
    -33, -32, -22, -13, -13, -22, -1, -14,
    -31, -28, -17, -12, -5, -7, 17, -3,
    -31, -23, -5, -5, 1, -7, 7, -22,
    -10, -8, 3, 8, 14, -3, 7, -9
};

const int eg_rook_table[64] = {
    16, 24, 32, 31, 24, 17, 16, 11,
    16, 28, 30, 20, 21, 8, 6, -4,
    14, 14, 15, 12, 0, -4, -14, -13,
    18, 15, 21, 15, 2, -3, -2, -7,
    14, 15, 14, 11, 8, 7, -3, -3,
    8, 6, 4, 6, 0, -7, -24, -19,
    2, 4, 3, 3, -4, -7, -18, -10,
    2, 2, 5, 1, -6, -1, -8, -9
};

const int mg_queen_table[64] = {
    -39, -55, -38, -19, -23, -17, 31, -20,
    -4, -33, -30, -41, -41, 2, -12, 37,
    -5, -10, -15, -7, -4, 27, 45, 27,
    -17, -12, -12, -13, -13, -2, 3, 8,
    -10, -15, -13, -6, -5, -6, 2, 7,
    -8, -2, -2, -3, 1, 5, 15, 8,
    -3, -1, 8, 10, 9, 12, 16, 29,
    -4, -5, 5, 9, 10, -12, 5, 1
};

const int eg_queen_table[64] = {
    20, 41, 58, 49, 52, 52, -5, 27,
    0, 29, 61, 81, 99, 59, 43, 31,
    1, 11, 41, 48, 66, 51, 18, 22,
    12, 18, 30, 50, 61, 53, 48, 36,
    4, 26, 26, 43, 40, 37, 30, 26,
    -7, 7, 17, 15, 21, 18, 5, 1,
    -12, -9, -12, -5, 0, -27, -55, -75,
    -15, -13, -16, 0, -19, -17, -40, -40
};

const int mg_king_table[64] = {
    50, 26, 48, -72, -38, 17, 79, 212,
    -86, -21, -54, 47, 1, 5, 45, 6,
    -99, 23, -50, -65, -23, 53, 28, -18,
    -69, -64, -85, -130, -121, -86, -80, -116,
    -78, -63, -89, -129, -121, -86, -92, -138,
    -36, -14, -58, -73, -67, -67, -36, -60,
    47, 6, -11, -42, -44, -31, 9, 26,
    48, 67, 38, -44, 14, -23, 49, 60
};

const int eg_king_table[64] = {
    -110, -54, -39, 7, -7, -8, -19, -135,
    -9, 18, 30, 15, 34, 46, 33, 4,
    4, 23, 44, 55, 55, 48, 44, 13,
    -6, 28, 50, 63, 65, 58, 45, 18,
    -14, 15, 40, 58, 56, 42, 28, 15,
    -25, 0, 22, 34, 34, 25, 7, -5,
    -43, -13, 0, 11, 14, 6, -12, -34,
    -84, -61, -35, -21, -39, -21, -51, -87
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

const int mgKingShieldBonus = 14;
const int egKingShieldBonus = -2;

const int mgIsolatedPawnPenalty = -15;
const int egIsolatedPawnPenalty = -14;

const int mgAggressivenessWeight = 75;
const int egAggressivenessWeight = -24;

const int passed_pawn_mg_table[64] = {
    0, 0, 0, 0, 0, 0, 0, 0,
    17, 19, 22, 22, 21, 20, 21, 17,
    30, 42, 25, 11, 12, 9, -21, -39,
    26, 23, 21, 18, 2, 13, -8, -9,
    10, 3, -16, -7, -15, -7, -11, -3,
    3, -8, -23, -19, -18, -11, -14, 7,
    -2, 1, -16, -17, -4, -3, 10, 2,
    0, 0, 0, 0, 0, 0, 0, 0
};

const int passed_pawn_eg_table[64] = {
    0, 0, 0, 0, 0, 0, 0, 0,
    67, 66, 67, 73, 74, 68, 64, 65,
    156, 153, 119, 70, 86, 105, 124, 150,
    87, 81, 52, 44, 43, 51, 80, 87,
    55, 46, 31, 21, 24, 30, 55, 52,
    16, 21, 13, 6, 5, 8, 35, 18,
    15, 16, 13, 8, -8, 2, 16, 18,
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
    
    int whiteAttackMg = (kingAttacks[WHITE] * kingAttacks[WHITE] * mgAggressivenessWeight) / 10;
    int blackAttackMg = (kingAttacks[BLACK] * kingAttacks[BLACK] * mgAggressivenessWeight) / 10;
    int whiteAttackEg = (kingAttacks[WHITE] * kingAttacks[WHITE] * egAggressivenessWeight) / 10;
    int blackAttackEg = (kingAttacks[BLACK] * kingAttacks[BLACK] * egAggressivenessWeight) / 10;

    int attackDiffMg = whiteAttackMg - blackAttackMg;
    int attackDiffEg = whiteAttackEg - blackAttackEg;

    if (side2move == BLACK) {
        attackDiffMg = -attackDiffMg;
        attackDiffEg = -attackDiffEg;
    }

    mgScore += attackDiffMg;
    egScore += attackDiffEg;

    return (mgScore * mgPhase + egScore * egPhase) / 24;
}

