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


const int mg_value[6] = { 69, 288, 322, 392, 833, 0 };
const int eg_value[6] = { 118, 341, 348, 621, 1168, 0 };

const int gamephaseInc[6] = { 0, 1, 1, 2, 4, 0 };

// Mobility bonuses and penalties
const int mgKnightMobility[] = { -28, -12, -5, 0, 2, 0, -1, -2, 0 };
const int egKnightMobility[] = { 12, 12, 18, 15, 18, 24, 25, 27, 23 };
const int mgBishopMobility[] = { -33, -27, -19, -16, -9, -2, 3, 7, 7, 10, 12, 16, 15, 42 };
const int egBishopMobility[] = { -41, -20, -13, -3, 10, 22, 26, 32, 40, 38, 36, 35, 39, 23 };
const int mgRookMobility[] = { -25, -16, -12, -9, -10, -3, 1, 9, 12, 18, 23, 28, 28, 32, 34 };
const int egRookMobility[] = { -21, -3, -1, 3, 10, 12, 15, 17, 25, 29, 30, 31, 38, 43, 39 };
const int mgQueenMobility[] = { -26, -22, -31, -27, -24, -22, -17, -19, -17, -15, -14, -14, -14, -13, -11, -11, -8, -11, -8, -6, 3, 7, 7, 12, 34, 91, 93, 132 };
const int egQueenMobility[] = { -121, -128, -50, -31, -25, -18, -12, 8, 15, 18, 28, 34, 42, 48, 52, 58, 62, 72, 77, 77, 75, 76, 78, 79, 59, 38, 34, 29 };

// Pesto tables
const int mg_pawn_table[64] = {
    0, 0, 0, 0, 0, 0, 0, 0,
    72, 87, 67, 100, 82, 63, -12, -28,
    -4, 0, 39, 45, 48, 69, 41, 6,
    -16, -6, 7, 12, 30, 23, 13, 3,
    -25, -15, 0, 16, 16, 13, 1, -8,
    -23, -18, -2, 2, 14, 9, 17, 3,
    -23, -17, -10, -13, 0, 9, 17, -18,
    0, 0, 0, 0, 0, 0, 0, 0
};

const int eg_pawn_table[64] = {
    0, 0, 0, 0, 0, 0, 0, 0,
    186, 178, 181, 125, 122, 138, 185, 196,
    120, 132, 98, 73, 63, 52, 100, 93,
    46, 37, 20, 8, -1, 5, 21, 19,
    19, 17, 2, -4, -6, -3, 7, -2,
    11, 15, 2, 8, 3, 2, 3, -8,
    14, 17, 9, 16, 19, 8, 2, -5,
    0, 0, 0, 0, 0, 0, 0, 0
};

const int mg_knight_table[64] = {
    -156, -131, -67, -38, -1, -57, -109, -104,
    -29, -13, 18, 42, 17, 80, -9, 19,
    -10, 29, 48, 57, 95, 96, 54, 20,
    -8, 8, 34, 56, 37, 61, 21, 27,
    -19, -3, 18, 21, 30, 24, 18, -7,
    -36, -14, 5, 13, 26, 12, 8, -17,
    -42, -34, -17, -3, -2, -9, -26, -28,
    -83, -26, -38, -22, -16, -24, -24, -64
};

const int eg_knight_table[64] = {
    -84, -18, -6, -15, -12, -36, -13, -105,
    -27, -7, -8, -9, -13, -30, -13, -44,
    -12, -5, 12, 13, -4, -9, -16, -23,
    -2, 10, 22, 23, 24, 19, 9, -11,
    -3, 2, 23, 23, 26, 15, 3, -11,
    -19, -4, 0, 17, 14, -4, -11, -16,
    -26, -12, -5, -4, -5, -8, -19, -15,
    -33, -38, -17, -14, -14, -20, -29, -39
};

const int mg_bishop_table[64] = {
    -28, -51, -47, -88, -72, -56, -20, -52,
    -18, -3, -7, -21, 5, 8, -4, -4,
    -3, 17, 13, 31, 22, 52, 38, 25,
    -9, 3, 16, 29, 24, 18, 2, -7,
    -8, -6, 0, 23, 18, 1, -5, 2,
    -2, 8, 9, 8, 10, 11, 10, 12,
    5, 11, 15, -4, 7, 8, 17, -2,
    -4, 16, 2, -9, -1, -14, 1, -5
};

const int eg_bishop_table[64] = {
    0, 9, 3, 16, 9, 0, -7, -3,
    -12, -3, -1, 0, -9, -11, 0, -15,
    11, 1, 5, -5, 0, 2, -3, 5,
    4, 10, 6, 16, 10, 9, 6, 3,
    -2, 9, 14, 11, 10, 9, 5, -10,
    2, 6, 9, 9, 12, 6, 1, -9,
    1, -6, -13, 5, 1, -4, 0, -12,
    -10, 1, -3, 3, 2, 13, -12, -19
};

const int mg_rook_table[64] = {
    18, 2, 4, 6, 22, 45, 42, 57,
    2, -5, 15, 37, 23, 53, 43, 76,
    -14, 5, 0, 3, 31, 38, 86, 57,
    -21, -15, -15, -8, -6, 4, 15, 15,
    -33, -35, -25, -17, -16, -22, -1, -13,
    -33, -31, -20, -15, -8, -8, 17, -4,
    -33, -26, -8, -8, -2, -9, 5, -24,
    -12, -11, 0, 5, 12, -6, 4, -11
};

const int eg_rook_table[64] = {
    13, 22, 30, 25, 18, 9, 8, 5,
    14, 27, 30, 19, 19, 6, 3, -10,
    14, 16, 17, 14, 1, -4, -12, -14,
    17, 15, 22, 16, 3, -2, -2, -6,
    11, 14, 14, 11, 8, 7, -4, -6,
    6, 5, 3, 5, 0, -8, -24, -22,
    0, 3, 3, 4, -4, -7, -17, -10,
    0, 2, 5, 1, -6, -1, -7, -9
};

const int mg_queen_table[64] = {
    -34, -45, -25, 9, 14, 22, 64, 10,
    -10, -37, -31, -41, -34, 12, -1, 51,
    -7, -14, -19, -7, 4, 42, 58, 53,
    -23, -18, -18, -19, -18, -5, 2, 8,
    -17, -22, -20, -12, -13, -12, -5, 2,
    -16, -10, -9, -10, -6, -2, 7, 2,
    -11, -9, 0, 2, 1, 4, 7, 21,
    -12, -13, -3, 2, 2, -21, -2, -7
};

const int eg_queen_table[64] = {
    15, 36, 53, 36, 35, 33, -22, 13,
    -4, 28, 59, 80, 95, 51, 35, 20,
    1, 12, 45, 51, 65, 47, 12, 15,
    10, 19, 30, 51, 64, 57, 49, 38,
    4, 26, 28, 43, 40, 38, 31, 28,
    -6, 8, 17, 15, 20, 18, 6, 1,
    -11, -8, -11, -3, 1, -24, -50, -73,
    -14, -12, -14, 3, -18, -12, -38, -39
};

const int mg_king_table[64] = {
    64, 38, 66, -80, -28, 22, 64, 181,
    -71, -28, -70, 37, -21, -11, 16, -4,
    -95, 10, -61, -76, -35, 37, 13, -22,
    -62, -78, -98, -144, -134, -97, -96, -117,
    -64, -77, -107, -142, -145, -105, -111, -126,
    -29, -23, -76, -91, -88, -85, -44, -48,
    59, 10, -9, -39, -44, -27, 14, 40,
    64, 82, 54, -28, 30, -7, 63, 78
};

const int eg_king_table[64] = {
    -108, -55, -43, 7, -9, -8, -17, -128,
    -9, 21, 33, 15, 38, 48, 40, 7,
    8, 27, 46, 57, 56, 50, 48, 18,
    -3, 32, 52, 65, 65, 59, 50, 23,
    -15, 18, 42, 58, 58, 44, 32, 14,
    -24, 1, 23, 35, 35, 26, 7, -8,
    -47, -17, -2, 8, 12, 3, -16, -37,
    -86, -65, -40, -25, -44, -26, -55, -89
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

const int doublePawnPenaltyOpening = -3;
const int doublePawnPenaltyEndgame = -32;

const int mgKingShieldBonus = 15;
const int egKingShieldBonus = -2;

const int mgIsolatedPawnPenalty = -16;
const int egIsolatedPawnPenalty = -3;

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

    // Isolated pawn penalty (& passed pawn bonus in the future maybe)
    Bitboard adjacent_file_masks[8] = {
        file_masks[1],                      // A -> B
        file_masks[0] | file_masks[2],      // B -> A, C
        file_masks[1] | file_masks[3],      // C -> B, D
        file_masks[2] | file_masks[4],      // D -> C, E
        file_masks[3] | file_masks[5],      // E -> D, F
        file_masks[4] | file_masks[6],      // F -> E, G
        file_masks[5] | file_masks[7],      // G -> F, H
        file_masks[6]                       // H -> G
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
                // Isolated pawn if no pawns on adjacent files
                if ((pawns & adjacent_file_masks[file]) == 0) {
                    mg[color] += mgIsolatedPawnPenalty * count;
                    eg[color] += egIsolatedPawnPenalty * count;
                }
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

