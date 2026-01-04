#include "evaluation.h"
#include <cmath>
#include <mutex>
#include "board.h"

#define WHITE 0
#define BLACK 1
#define PAWN   0
#define KNIGHT 1
#define BISHOP 2
#define ROOK   3
#define QUEEN  4
#define KING   5

#define FLIP(sq) ((sq)^56)
#define OTHER(side) ((side)^ 1)


int mg_value[6] = { 82, 337, 365, 477, 1025,  0};
int eg_value[6] = { 94, 281, 297, 512,  936,  0};

// Standard centipawn material values aligned with Board's +/- piece encoding
const int PIECE_VALUES[7] = {0, 100, 320, 330, 500, 900, 20000};
// Opening nudges for better central play
constexpr int CENTER_STRONG_PAWN_BONUS = 40;   // d4/e4 (and mirrored)
constexpr int CENTER_SOFT_PAWN_BONUS   = 18;   // d3/e3 (and mirrored)
constexpr int CENTER_KNIGHT_BONUS      = 14;   // c3/f3 (c6/f6)
constexpr int CENTER_KNIGHT_SOFT       = 8;    // squares that attack center (d2/e2/d7/e7 etc)
constexpr int TEMPO_MG = 15;

inline int abs_int(int x) {
    return x < 0 ? -x : x;
}

// Distance from center squares (3,3), (3,4), (4,3), (4,4)
int center_distance(int row, int col) {
    return std::abs(2 * row - 7) + std::abs(2 * col - 7);
}

// Distance between two squares
int manhattan_distance(int r1, int c1, int r2, int c2) {
    return std::abs(r1 - r2) + std::abs(c1 - c2);
}

int mg_pawn_table[64] = {
      0,   0,   0,   0,   0,   0,  0,   0,
     98, 134,  61,  95,  68, 126, 34, -11,
     -6,   7,  26,  31,  65,  56, 25, -20,
    -14,  13,   6,  21,  23,  12, 17, -23,
    -27,  -2,  -5,  12,  17,   6, 10, -25,
    -26,  -4,  -4, -10,   3,   3, 33, -12,
    -35,  -1, -20, -23, -15,  24, 38, -22,
      0,   0,   0,   0,   0,   0,  0,   0,
};

int eg_pawn_table[64] = {
      0,   0,   0,   0,   0,   0,   0,   0,
    178, 173, 158, 134, 147, 132, 165, 187,
     94, 100,  85,  67,  56,  53,  82,  84,
     32,  24,  13,   5,  -2,   4,  17,  17,
     13,   9,  -3,  -7,  -7,  -8,   3,  -1,
      4,   7,  -6,   1,   0,  -5,  -1,  -8,
     13,   8,   8,  10,  13,   0,   2,  -7,
      0,   0,   0,   0,   0,   0,   0,   0,
};

int mg_knight_table[64] = {
    -167, -89, -34, -49,  61, -97, -15, -107,
     -73, -41,  72,  36,  23,  62,   7,  -17,
     -47,  60,  37,  65,  84, 129,  73,   44,
      -9,  17,  19,  53,  37,  69,  18,   22,
     -13,   4,  16,  13,  28,  19,  21,   -8,
     -23,  -9,  12,  10,  19,  17,  25,  -16,
     -29, -53, -12,  -3,  -1,  18, -14,  -19,
    -105, -21, -58, -33, -17, -28, -19,  -23,
};

int eg_knight_table[64] = {
    -58, -38, -13, -28, -31, -27, -63, -99,
    -25,  -8, -25,  -2,  -9, -25, -24, -52,
    -24, -20,  10,   9,  -1,  -9, -19, -41,
    -17,   3,  22,  22,  22,  11,   8, -18,
    -18,  -6,  16,  25,  16,  17,   4, -18,
    -23,  -3,  -1,  15,  10,  -3, -20, -22,
    -42, -20, -10,  -5,  -2, -20, -23, -44,
    -29, -51, -23, -15, -22, -18, -50, -64,
};

int mg_bishop_table[64] = {
    -29,   4, -82, -37, -25, -42,   7,  -8,
    -26,  16, -18, -13,  30,  59,  18, -47,
    -16,  37,  43,  40,  35,  50,  37,  -2,
     -4,   5,  19,  50,  37,  37,   7,  -2,
     -6,  13,  13,  26,  34,  12,  10,   4,
      0,  15,  15,  15,  14,  27,  18,  10,
      4,  15,  16,   0,   7,  21,  33,   1,
    -33,  -3, -14, -21, -13, -12, -39, -21,
};

int eg_bishop_table[64] = {
    -14, -21, -11,  -8, -7,  -9, -17, -24,
     -8,  -4,   7, -12, -3, -13,  -4, -14,
      2,  -8,   0,  -1, -2,   6,   0,   4,
     -3,   9,  12,   9, 14,  10,   3,   2,
     -6,   3,  13,  19,  7,  10,  -3,  -9,
    -12,  -3,   8,  10, 13,   3,  -7, -15,
    -14, -18,  -7,  -1,  4,  -9, -15, -27,
    -23,  -9, -23,  -5, -9, -16,  -5, -17,
};

int mg_rook_table[64] = {
     32,  42,  32,  51, 63,  9,  31,  43,
     27,  32,  58,  62, 80, 67,  26,  44,
     -5,  19,  26,  36, 17, 45,  61,  16,
    -24, -11,   7,  26, 24, 35,  -8, -20,
    -36, -26, -12,  -1,  9, -7,   6, -23,
    -45, -25, -16, -17,  3,  0,  -5, -33,
    -44, -16, -20,  -9, -1, 11,  -6, -71,
    -19, -13,   1,  17, 16,  7, -37, -26,
};

int eg_rook_table[64] = {
    13, 10, 18, 15, 12,  12,   8,   5,
    11, 13, 13, 11, -3,   3,   8,   3,
     7,  7,  7,  5,  4,  -3,  -5,  -3,
     4,  3, 13,  1,  2,   1,  -1,   2,
     3,  5,  8,  4, -5,  -6,  -8, -11,
    -4,  0, -5, -1, -7, -12,  -8, -16,
    -6, -6,  0,  2, -9,  -9, -11,  -3,
    -9,  2,  3, -1, -5, -13,   4, -20,
};

int mg_queen_table[64] = {
    -28,   0,  29,  12,  59,  44,  43,  45,
    -24, -39,  -5,   1, -16,  57,  28,  54,
    -13, -17,   7,   8,  29,  56,  47,  57,
    -27, -27, -16, -16,  -1,  17,  -2,   1,
     -9, -26,  -9, -10,  -2,  -4,   3,  -3,
    -14,   2, -11,  -2,  -5,   2,  14,   5,
    -35,  -8,  11,   2,   8,  15,  -3,   1,
     -1, -18,  -9,  10, -15, -25, -31, -50,
};

int eg_queen_table[64] = {
     -9,  22,  22,  27,  27,  19,  10,  20,
    -17,  20,  32,  41,  58,  25,  30,   0,
    -20,   6,   9,  49,  47,  35,  19,   9,
      3,  22,  24,  45,  57,  40,  57,  36,
    -18,  28,  19,  47,  31,  34,  39,  23,
    -16, -27,  15,   6,   9,  17,  10,   5,
    -22, -23, -30, -16, -16, -23, -36, -32,
    -33, -28, -22, -43,  -5, -32, -20, -41,
};

int mg_king_table[64] = {
    -65,  23,  16, -15, -56, -34,   2,  13,
     29,  -1, -20,  -7,  -8,  -4, -38, -29,
     -9,  24,   2, -16, -20,   6,  22, -22,
    -17, -20, -12, -27, -30, -25, -14, -36,
    -49,  -1, -27, -39, -46, -44, -33, -51,
    -14, -14, -22, -46, -44, -30, -15, -27,
      1,   7,  -8, -64, -43, -16,   9,   8,
    -15,  36,  12, -54,   8, -28,  24,  14,
};

int eg_king_table[64] = {
    -74, -35, -18, -18, -11,  15,   4, -17,
    -12,  17,  14,  17,  17,  38,  23,  11,
     10,  17,  23,  15,  20,  45,  44,  13,
     -8,  22,  24,  27,  26,  33,  26,   3,
    -18,  -4,  21,  24,  27,  23,   9, -11,
    -19,  -3,  11,  21,  23,  16,   7,  -9,
    -27, -11,   4,  13,  14,   4,  -5, -17,
    -53, -34, -21, -11, -28, -14, -24, -43
};

inline int to_sq(int row, int col) {
    // Tahtan (0=A1) ile Pesto (0=A8) ters olduğu için çeviriyoruz.
    // Row 2 (Piyon) -> (7-2)=5. satıra (Index 40 civarı) denk gelecek.
    return (7 - row) * 8 + col;
}
inline int flip_sq(int sq) {
    // Vertical flip (A1<->A8 etc). Matches the classic FLIP(sq) = sq ^ 56.
    return sq ^ 56;
}

int* mg_pesto_table[6] =
{
    mg_pawn_table,
    mg_knight_table,
    mg_bishop_table,
    mg_rook_table,
    mg_queen_table,
    mg_king_table
};

int* eg_pesto_table[6] =
{
    eg_pawn_table,
    eg_knight_table,
    eg_bishop_table,
    eg_rook_table,
    eg_queen_table,
    eg_king_table
};

int gamephaseIncType[6] = {0, 1, 1, 2, 4, 0}; // pawn, knight, bishop, rook, queen, king
int mg_table[12][64]; // first six: white pieces, next six: black pieces
int eg_table[12][64];

void init_tables();

namespace {
    std::once_flag tables_once;

    inline int piece_to_table_index(int piece) {
        // Map Board encoding (+/-1..6) to 0..11 pesto table index
        const int absPiece = std::abs(piece);
        if (absPiece < pawn || absPiece > king) return -1;
        int idx = absPiece - 1;
        if (piece < 0) idx += 6;
        return idx;
    }

    int compute_game_phase(const Board& board) {
        int phase = 0;
        for (int r = 0; r < 8; ++r) {
            for (int c = 0; c < 8; ++c) {
                const int piece = board.squares[r][c];
                if (piece == 0) continue;
                const int absPiece = std::abs(piece);
                if (absPiece < pawn || absPiece > king) continue;
                phase += gamephaseIncType[absPiece - 1];
            }
        }
        return phase;
    }

    void ensure_tables_init() {
        std::call_once(tables_once, []() {
            init_tables();
        });
    }
}

void init_tables()
{
    for (int p = 0; p < 6; p++) {
        for (int sq = 0; sq < 64; sq++) {

            mg_table[p][sq] = mg_pesto_table[p][sq] + mg_value[p];
            eg_table[p][sq] = eg_pesto_table[p][sq] + eg_value[p];

            
            int mirror_sq = flip_sq(sq); 

            mg_table[p + 6][sq] = mg_pesto_table[p][mirror_sq] + mg_value[p];
            eg_table[p + 6][sq] = eg_pesto_table[p][mirror_sq] + eg_value[p];
        }
    }
}

bool is_endgame(const Board& board) {
    const int phase = compute_game_phase(board);

    int whiteQueens = 0, blackQueens = 0;
    int nonPawnMaterial = 0;

    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 8; ++c) {
            const int piece = board.squares[r][c];
            if (piece == 0) continue;

            const int absPiece = std::abs(piece);
            if (absPiece == queen) {
                (piece > 0 ? whiteQueens : blackQueens)++;
            }
            if (absPiece != pawn && absPiece != king) {
                nonPawnMaterial += PIECE_VALUES[absPiece];
            }
        }
    }

    const bool noQueens = (whiteQueens + blackQueens) == 0;
    const bool singleQueen = (whiteQueens + blackQueens) == 1;

    if (phase <= 8) return true; // Material almost gone
    if (noQueens && nonPawnMaterial <= 2600) return true; // Only rooks/minors left
    if (singleQueen && nonPawnMaterial <= 1800) return true; // Lone queen vs light pieces

    return false;
}

int evaluate_board(const Board& board) {
    ensure_tables_init();

    int mg[2] = {0, 0};
    int eg[2] = {0, 0};
    int gamePhase = 0;
    int centerControl[2] = {0, 0};

    /* evaluate each piece */
    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 8; ++c) {
            const int piece = board.squares[r][c];
            if (piece == 0) continue;

            const int idx = piece_to_table_index(piece);
            if (idx < 0) continue;

            const int pesto_sq = to_sq(r, c);
            const int color = piece > 0 ? WHITE : BLACK;

            mg[color] += mg_table[idx][pesto_sq];
            eg[color] += eg_table[idx][pesto_sq];

            const int absPiece = std::abs(piece);
            if (absPiece >= pawn && absPiece <= king) {
                gamePhase += gamephaseIncType[absPiece - 1];
            }

            // Light central control nudges to prefer d/e pawn pushes and knight development.
            if (absPiece == pawn) {
                // Strong: d4/e4 for side to move (mirrored by color via pesto_sq index)
                if (pesto_sq == 27 || pesto_sq == 28 || pesto_sq == 35 || pesto_sq == 36) {
                    centerControl[color] += CENTER_STRONG_PAWN_BONUS;
                }
                // Soft: d3/e3/c4/f4 (and mirrors) to gently prefer early central steps over f3/a3
                else if (pesto_sq == 19 || pesto_sq == 20 || pesto_sq == 43 || pesto_sq == 44 ||
                         pesto_sq == 26 || pesto_sq == 29 || pesto_sq == 34 || pesto_sq == 37) {
                    centerControl[color] += CENTER_SOFT_PAWN_BONUS;
                }
            } else if (absPiece == knight) {
                // Natural development squares hitting center
                if (pesto_sq == 18 || pesto_sq == 21 || pesto_sq == 42 || pesto_sq == 45) {
                    centerControl[color] += CENTER_KNIGHT_BONUS; // c3/f3/c6/f6
                } else if (pesto_sq == 17 || pesto_sq == 22 || pesto_sq == 41 || pesto_sq == 46) {
                    centerControl[color] += CENTER_KNIGHT_SOFT; // squares that still point to center
                }
            }
        }
    }

    /* tapered eval from White's perspective */
    int mgScore = mg[WHITE] - mg[BLACK];
    int egScore = eg[WHITE] - eg[BLACK];

    // Add modest opening-oriented center bonus and a small tempo term.
    mgScore += centerControl[WHITE] - centerControl[BLACK];
    egScore += (centerControl[WHITE] - centerControl[BLACK]) / 3; // fade harder in endgame

    // Small tempo only in middlegame to avoid skewing deep endgames
    mgScore += board.isWhiteTurn ? TEMPO_MG : -TEMPO_MG;
    
    int mgPhase = gamePhase;
    if (mgPhase > 24) mgPhase = 24; 
    
    int egPhase = 24 - mgPhase;
    
    // (mg * phase + eg * (24-phase)) / 24
    return (mgScore * mgPhase + egScore * egPhase) / 24;
}

int evaluate_board_pesto(const Board& board) {
    // Wrapper kept for compatibility; pesto tables back the main evaluator.
    return evaluate_board(board);
}

int repetition_draw_score(const Board& board) {
    int currentScore = evaluate_board(board);
    if (!board.isWhiteTurn) currentScore = -currentScore;
    
    // if my position is clearly better, then a draw is bad
    if (currentScore > 100) return -50; 
    
    // if my position is clearly worse, then a draw is good
    return 0;
}
