#include "evaluation.h"
#include <cmath>
#include "board.h"

const int PIECE_VALUES[7] = {0, 100, 300, 300, 500, 900, 20000};

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

namespace {
constexpr int PESTO_TEMPO_BONUS = 10;
constexpr int PESTO_MAX_PHASE = 24;

// Classic PESTO (Rofchade) piece values (centipawns): pawn..king.
constexpr int PESTO_MG_VALUE[6] = {82, 337, 365, 477, 1025, 0};
constexpr int PESTO_EG_VALUE[6] = {94, 281, 297, 512, 936, 0};

// Game phase increments per piece type (pawn..king).
constexpr int PESTO_PHASE_INC[6] = {0, 1, 1, 2, 4, 0};

// Passed-pawn bonuses must be modest: PESTO PSTs already reward advanced pawns.
// These values are in centipawns, tuned to stay within typical PESTO scale.
// Index is "rank from own side" using: rank = (7-row) for white, rank = row for black.
constexpr int PASSED_PAWN_MG_BONUS[8] = {0, 5, 10, 20, 35, 55, 80, 0};
constexpr int PASSED_PAWN_EG_BONUS[8] = {0, 10, 20, 35, 55, 80, 120, 0};

inline int clamp_int(int x, int lo, int hi) {
    return (x < lo) ? lo : (x > hi) ? hi : x;
}

inline int to_sq_a1(int row, int col) {
    // Convert (row,col) from board.squares[8][8] to a 0..63 square index where:
    // 0 = a1, 7 = h1, 56 = a8, 63 = h8
    return (7 - row) * 8 + col;
}

inline int flip_sq(int sq) {
    // Vertical flip (A1<->A8 etc). Matches the classic FLIP(sq) = sq ^ 56.
    return sq ^ 56;
}

inline bool has_pawn_on_file(const Board& board, int file, bool whitePawn) {
    const int pawnPiece = whitePawn ? pawn : -pawn;
    for (int r = 0; r < 8; r++) {
        if (board.squares[r][file] == pawnPiece) return true;
    }
    return false;
}

inline bool is_passed_pawn(const Board& board, int row, int col, bool isWhitePawn) {
    const int enemyPawn = isWhitePawn ? -pawn : pawn;
    const int dir = isWhitePawn ? -1 : 1;
    for (int dc = -1; dc <= 1; dc++) {
        const int file = col + dc;
        if (file < 0 || file > 7) continue;
        for (int r = row + dir; r >= 0 && r < 8; r += dir) {
            if (board.squares[r][file] == enemyPawn) return false;
        }
    }
    return true;
}

inline bool pawn_path_blocked(const Board& board, int row, int col, bool isWhitePawn) {
    const int dir = isWhitePawn ? -1 : 1;
    for (int r = row + dir; r >= 0 && r < 8; r += dir) {
        if (board.squares[r][col] != 0) return true;
    }
    return false;
}

// Values from Rofchade. Oriented for white, using a1-indexing.
constexpr int pesto_mg_pawn_table[64] = {
      0,   0,   0,   0,   0,   0,   0,   0,
     98, 134,  61,  95,  68, 126,  34, -11,
     -6,   7,  26,  31,  65,  56,  25, -20,
    -14,  13,   6,  21,  23,  12,  17, -23,
    -27,  -2,  -5,  12,  17,   6,  10, -25,
    -26,  -4,  -4, -10,   3,   3,  33, -12,
    -35,  -1, -20, -23, -15,  24,  38, -22,
      0,   0,   0,   0,   0,   0,   0,   0,
};

constexpr int pesto_eg_pawn_table[64] = {
      0,   0,   0,   0,   0,   0,   0,   0,
    178, 173, 158, 134, 147, 132, 165, 187,
     94, 100,  85,  67,  56,  53,  82,  84,
     32,  24,  13,   5,  -2,   4,  17,  17,
     13,   9,  -3,  -7,  -7,  -8,   3,  -1,
      4,   7,  -6,   1,   0,  -5,  -1,  -8,
     13,   8,   8,  10,  13,   0,   2,  -7,
      0,   0,   0,   0,   0,   0,   0,   0,
};

constexpr int pesto_mg_knight_table[64] = {
    -167, -89, -34, -49,  61, -97, -15, -107,
     -73, -41,  72,  36,  23,  62,   7,  -17,
     -47,  60,  37,  65,  84, 129,  73,   44,
      -9,  17,  19,  53,  37,  69,  18,   22,
     -13,   4,  16,  13,  28,  19,  21,   -8,
     -23,  -9,  12,  10,  19,  17,  25,  -16,
     -29, -53, -12,  -3,  -1,  18, -14,  -19,
    -105, -21, -58, -33, -17, -28, -19,  -23,
};

constexpr int pesto_eg_knight_table[64] = {
    -58, -38, -13, -28, -31, -27, -63, -99,
    -25,  -8, -25,  -2,  -9, -25, -24, -52,
    -24, -20,  10,   9,  -1,  -9, -19, -41,
    -17,   3,  22,  22,  22,  11,   8, -18,
    -18,  -6,  16,  25,  16,  17,   4, -18,
    -23,  -3,  -1,  15,  10,  -3, -20, -22,
    -42, -20, -10,  -5,  -2, -20, -23, -44,
    -29, -51, -23, -15, -22, -18, -50, -64,
};

constexpr int pesto_mg_bishop_table[64] = {
    -29,   4, -82, -37, -25, -42,   7,  -8,
    -26,  16, -18, -13,  30,  59,  18, -47,
    -16,  37,  43,  40,  35,  50,  37,  -2,
     -4,   5,  19,  50,  37,  37,   7,  -2,
     -6,  13,  13,  26,  34,  12,  10,   4,
      0,  15,  15,  15,  14,  27,  18,  10,
      4,  15,  16,   0,   7,  21,  33,   1,
    -33,  -3, -14, -21, -13, -12, -39, -21,
};

constexpr int pesto_eg_bishop_table[64] = {
    -14, -21, -11,  -8,  -7,  -9, -17, -24,
     -8,  -4,   7, -12,  -3, -13,  -4, -14,
      2,  -8,   0,  -1,  -2,   6,   0,   4,
     -3,   9,  12,   9,  14,  10,   3,   2,
     -6,   3,  13,  19,   7,  10,  -3,  -9,
    -12,  -3,   8,  10,  13,   3,  -7, -15,
    -14, -18,  -7,  -1,   4,  -9, -15, -27,
    -23,  -9, -23,  -5,  -9, -16,  -5, -17,
};

constexpr int pesto_mg_rook_table[64] = {
     32,  42,  32,  51,  63,   9,  31,  43,
     27,  32,  58,  62,  80,  67,  26,  44,
     -5,  19,  26,  36,  17,  45,  61,  16,
    -24, -11,   7,  26,  24,  35,  -8, -20,
    -36, -26, -12,  -1,   9,  -7,   6, -23,
    -45, -25, -16, -17,   3,   0,  -5, -33,
    -44, -16, -20,  -9,  -1,  11,  -6, -71,
    -19, -13,   1,  17,  16,   7, -37, -26,
};

constexpr int pesto_eg_rook_table[64] = {
    13, 10, 18, 15, 12, 12,  8,  5,
    11, 13, 13, 11, -3,  3,  8,  3,
     7,  7,  7,  5,  4, -3, -5, -3,
     4,  3, 13,  1,  2,  1, -1,  2,
     3,  5,  8,  4, -5, -6, -8, -11,
    -4,  0, -5, -1, -7, -12, -8, -16,
    -6, -6,  0,  2, -9,  -9, -11, -3,
    -9,  2,  3, -1, -5, -13,  4, -20,
};

constexpr int pesto_mg_queen_table[64] = {
    -28,   0,  29,  12,  59,  44,  43,  45,
    -24, -39,  -5,   1, -16,  57,  28,  54,
    -13, -17,   7,   8,  29,  56,  47,  57,
    -27, -27, -16, -16,  -1,  17,  -2,   1,
     -9, -26,  -9, -10,  -2,  -4,   3,  -3,
    -14,   2, -11,  -2,  -5,   2,  14,   5,
    -35,  -8,  11,   2,   8,  15,  -3,   1,
     -1, -18,  -9,  10, -15, -25, -31, -50,
};

constexpr int pesto_eg_queen_table[64] = {
     -9,  22,  22,  27,  27,  19,  10,  20,
    -17,  20,  32,  41,  58,  25,  30,   0,
    -20,   6,   9,  49,  47,  35,  19,   9,
      3,  22,  24,  45,  57,  40,  57,  36,
    -18,  28,  19,  47,  31,  34,  39,  23,
    -16, -27,  15,   6,   9,  17,  10,   5,
    -22, -23, -30, -16, -16, -23, -36, -32,
    -33, -28, -22, -43,  -5, -32, -20, -41,
};

constexpr int pesto_mg_king_table[64] = {
    -65,  23,  16, -15, -56, -34,   2,  13,
     29,  -1, -20,  -7,  -8,  -4, -38, -29,
     -9,  24,   2, -16, -20,   6,  22, -22,
    -17, -20, -12, -27, -30, -25, -14, -36,
    -49,  -1, -27, -39, -46, -44, -33, -51,
    -14, -14, -22, -46, -44, -30, -15, -27,
      1,   7,  -8, -64, -43, -16,   9,   8,
    -15,  36,  12, -54,   8, -28,  24,  14,
};

constexpr int pesto_eg_king_table[64] = {
    -74, -35, -18, -18, -11,  15,   4, -17,
    -12,  17,  14,  17,  17,  38,  23,  11,
     10,  17,  23,  15,  20,  45,  44,  13,
     -8,  22,  24,  27,  26,  33,  26,   3,
    -18,  -4,  21,  24,  27,  23,   9, -11,
    -19,  -3,  11,  21,  23,  16,   7,  -9,
    -27, -11,   4,  13,  14,   4,  -5, -17,
    -53, -34, -21, -11, -28, -14, -24, -43,
};

inline int pesto_mg_pst(int pieceIndex, int sq) {
    switch (pieceIndex) {
        case 0: return pesto_mg_pawn_table[sq];
        case 1: return pesto_mg_knight_table[sq];
        case 2: return pesto_mg_bishop_table[sq];
        case 3: return pesto_mg_rook_table[sq];
        case 4: return pesto_mg_queen_table[sq];
        case 5: return pesto_mg_king_table[sq];
        default: return 0;
    }
}

inline int pesto_eg_pst(int pieceIndex, int sq) {
    switch (pieceIndex) {
        case 0: return pesto_eg_pawn_table[sq];
        case 1: return pesto_eg_knight_table[sq];
        case 2: return pesto_eg_bishop_table[sq];
        case 3: return pesto_eg_rook_table[sq];
        case 4: return pesto_eg_queen_table[sq];
        case 5: return pesto_eg_king_table[sq];
        default: return 0;
    }
}
} // namespace

int evaluate_board_pesto(const Board& board) {
    int mgScore = 0;
    int egScore = 0;
    int phase = 0;

    int whiteBishops = 0;
    int blackBishops = 0;

    int whitePawnsPerFile[8] = {0};
    int blackPawnsPerFile[8] = {0};

    int whiteRookCount = 0;
    int blackRookCount = 0;
    int whiteRooksRow[2] = {0, 0};
    int whiteRooksCol[2] = {0, 0};
    int blackRooksRow[2] = {0, 0};
    int blackRooksCol[2] = {0, 0};

    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            const int p = board.squares[row][col];
            if (p == 0) continue;
            const int absPiece = abs_int(p);
            const int sign = (p > 0) ? 1 : -1;

            // Our piece constants are pawn..king = 1..6; PESTO arrays are 0..5.
            const int pieceIndex = absPiece - 1;

            // Convert to a1-indexed square and flip for black pieces.
            const int sq = to_sq_a1(row, col);
            const int pstSq = (p > 0) ? sq : flip_sq(sq);

            mgScore += sign * (PESTO_MG_VALUE[pieceIndex] + pesto_mg_pst(pieceIndex, pstSq));
            egScore += sign * (PESTO_EG_VALUE[pieceIndex] + pesto_eg_pst(pieceIndex, pstSq));
            phase += PESTO_PHASE_INC[pieceIndex];

            if (absPiece == bishop) {
                if (p > 0) whiteBishops++; else blackBishops++;
            } else if (absPiece == pawn) {
                if (p > 0) whitePawnsPerFile[col]++; else blackPawnsPerFile[col]++;
            } else if (absPiece == rook) {
                if (p > 0) {
                    if (whiteRookCount < 2) { whiteRooksRow[whiteRookCount] = row; whiteRooksCol[whiteRookCount] = col; }
                    whiteRookCount++;
                } else {
                    if (blackRookCount < 2) { blackRooksRow[blackRookCount] = row; blackRooksCol[blackRookCount] = col; }
                    blackRookCount++;
                }
            }
        }
    }

    // Pawn structure: isolated, doubled and passed
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            const int p = board.squares[row][col];
            if (abs_int(p) != pawn) continue;
            const bool isWhitePawn = (p > 0);
            const int sign = isWhitePawn ? 1 : -1;

            const int* myFiles = isWhitePawn ? whitePawnsPerFile : blackPawnsPerFile;

            bool isolated = true;
            if (col > 0 && myFiles[col - 1] > 0) isolated = false;
            if (col < 7 && myFiles[col + 1] > 0) isolated = false;
            if (isolated) {
                mgScore += sign * -15;
                egScore += sign * -10;
            }

            if (myFiles[col] > 1) {
                // Apply a modest penalty per pawn on a doubled file.
                mgScore += sign * -12;
                egScore += sign * -6;
            }

            if (is_passed_pawn(board, row, col, isWhitePawn)) {
                const int rank = clamp_int(isWhitePawn ? (7 - row) : row, 0, 7);
                int mgBonus = PASSED_PAWN_MG_BONUS[rank];
                int egBonus = PASSED_PAWN_EG_BONUS[rank];
                if (pawn_path_blocked(board, row, col, isWhitePawn)) {
                    // If it can't advance right now, it shouldn't dominate the eval.
                    mgBonus /= 2;
                    egBonus /= 4;
                }
                mgScore += sign * mgBonus;
                egScore += sign * egBonus;
            }
        }
    }

    // Rooks: open/semi-open file bonuses
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            const int p = board.squares[row][col];
            if (abs_int(p) != rook) continue;
            const bool isWhiteRook = (p > 0);
            const int sign = isWhiteRook ? 1 : -1;

            const bool hasWhitePawn = has_pawn_on_file(board, col, true);
            const bool hasBlackPawn = has_pawn_on_file(board, col, false);

            const bool openFile = !hasWhitePawn && !hasBlackPawn;
            const bool semiOpenFile = openFile ? false : (isWhiteRook ? !hasWhitePawn : !hasBlackPawn);

            if (openFile) {
                mgScore += sign * 50;
                egScore += sign * 25;
            } else if (semiOpenFile) {
                mgScore += sign * 25;
                egScore += sign * 10;
            }
        }
    }

    // Connected rooks: one bonus per connected pair
    auto connected_rooks_bonus = [&](bool white) {
        const int cnt = white ? whiteRookCount : blackRookCount;
        if (cnt < 2) return 0;

        const int r1 = white ? whiteRooksRow[0] : blackRooksRow[0];
        const int c1 = white ? whiteRooksCol[0] : blackRooksCol[0];
        const int r2 = white ? whiteRooksRow[1] : blackRooksRow[1];
        const int c2 = white ? whiteRooksCol[1] : blackRooksCol[1];

        // Same file
        if (c1 == c2) {
            const int lo = (r1 < r2) ? r1 : r2;
            const int hi = (r1 < r2) ? r2 : r1;
            bool clear = true;
            for (int r = lo + 1; r < hi; r++) {
                if (board.squares[r][c1] != 0) { clear = false; break; }
            }
            if (clear) return 30;
        }

        // Same rank
        if (r1 == r2) {
            const int lo = (c1 < c2) ? c1 : c2;
            const int hi = (c1 < c2) ? c2 : c1;
            bool clear = true;
            for (int c = lo + 1; c < hi; c++) {
                if (board.squares[r1][c] != 0) { clear = false; break; }
            }
            if (clear) return 30;
        }
        return 0;
    };

    mgScore += connected_rooks_bonus(true);
    mgScore -= connected_rooks_bonus(false);

    // Bishop pair
    if (whiteBishops >= 2) { mgScore += 30; egScore += 60; }
    if (blackBishops >= 2) { mgScore -= 30; egScore -= 60; }

    // King safety + center control are mainly midgame terms
    const bool endgame = is_endgame(board);
    if (!endgame) {
        // Pawn shield
        {
            const int wKingFile = board.whiteKingCol;
            int pawnShield = 0;
            for (int dc = -1; dc <= 1; dc++) {
                const int file = wKingFile + dc;
                if (file < 0 || file > 7) continue;
                if (board.squares[6][file] == pawn) pawnShield += 20;
                else if (board.squares[5][file] == pawn) pawnShield += 10;
            }
            mgScore += pawnShield;

            const int bKingFile = board.blackKingCol;
            int blackPawnShield = 0;
            for (int dc = -1; dc <= 1; dc++) {
                const int file = bKingFile + dc;
                if (file < 0 || file > 7) continue;
                if (board.squares[1][file] == -pawn) blackPawnShield += 20;
                else if (board.squares[2][file] == -pawn) blackPawnShield += 10;
            }
            mgScore -= blackPawnShield;

            // Open file is a danger next to king (no own pawn on that file)
            int openFilePenalty = 0;
            for (int dc = -1; dc <= 1; dc++) {
                const int file = wKingFile + dc;
                if (file < 0 || file > 7) continue;
                if (!has_pawn_on_file(board, file, true)) openFilePenalty += 50;
            }
            mgScore -= openFilePenalty;

            int blackOpenFilePenalty = 0;
            for (int dc = -1; dc <= 1; dc++) {
                const int file = bKingFile + dc;
                if (file < 0 || file > 7) continue;
                if (!has_pawn_on_file(board, file, false)) blackOpenFilePenalty += 50;
            }
            mgScore += blackOpenFilePenalty;
        }

        // Queen proximity to king
        {
            int queenThreat = 0;
            for (int r = 0; r < 8; r++) {
                for (int c = 0; c < 8; c++) {
                    if (board.squares[r][c] == -queen) {
                        const int d = std::abs(r - board.whiteKingRow) + std::abs(c - board.whiteKingCol);
                        if (d <= 3) queenThreat += (4 - d) * 40;
                    }
                }
            }
            mgScore -= queenThreat;

            int blackQueenThreat = 0;
            for (int r = 0; r < 8; r++) {
                for (int c = 0; c < 8; c++) {
                    if (board.squares[r][c] == queen) {
                        const int d = std::abs(r - board.blackKingRow) + std::abs(c - board.blackKingCol);
                        if (d <= 3) blackQueenThreat += (4 - d) * 40;
                    }
                }
            }
            mgScore += blackQueenThreat;
        }

        // NOTE: No manual "center control" bonus here.
        // PESTO PSTs already encode centralization incentives; adding another layer
        // tends to double-count and can cause suicidal center obsession.
    }

    // Mop-up (endgame conversion assistance)
    // Applies in endgames where one side is clearly ahead.
    // We add this to EG score so it fades out automatically as MG dominates.
    {
        const int tmpPhase = clamp_int(phase, 0, PESTO_MAX_PHASE);
        const int blendedNoTempo = (mgScore * tmpPhase + egScore * (PESTO_MAX_PHASE - tmpPhase)) / PESTO_MAX_PHASE;
        if (endgame && (blendedNoTempo > 200 || blendedNoTempo < -200)) {
            const bool whiteWinning = (blendedNoTempo > 0);
            const int winningKingRow = whiteWinning ? board.whiteKingRow : board.blackKingRow;
            const int winningKingCol = whiteWinning ? board.whiteKingCol : board.blackKingCol;
            const int losingKingRow  = whiteWinning ? board.blackKingRow : board.whiteKingRow;
            const int losingKingCol  = whiteWinning ? board.blackKingCol : board.whiteKingCol;

            const int enemyDistFromCenter = center_distance(losingKingRow, losingKingCol);
            const int distanceBetweenKings = manhattan_distance(
                winningKingRow, winningKingCol,
                losingKingRow, losingKingCol
            );

            int mopUpScore = 0;
            mopUpScore += enemyDistFromCenter * 10;
            mopUpScore += (14 - distanceBetweenKings) * 5;
            egScore += whiteWinning ? mopUpScore : -mopUpScore;
        }
    }

    phase = clamp_int(phase, 0, PESTO_MAX_PHASE);
    const int score = (mgScore * phase + egScore * (PESTO_MAX_PHASE - phase)) / PESTO_MAX_PHASE;
    return score + (board.isWhiteTurn ? PESTO_TEMPO_BONUS : -PESTO_TEMPO_BONUS);
}

bool is_endgame(const Board& board) {
    int whiteQueens = 0;
    int blackQueens = 0;
    int whiteRooks = 0;
    int blackRooks = 0;
    int whiteOther = 0; // non-pawn, non-king, non-queen pieces
    int blackOther = 0;
    int whiteMinors = 0; // knights + bishops
    int blackMinors = 0;

    for (int r = 0; r < 8; r++) {
        for (int c = 0; c < 8; c++) {
            int p = board.squares[r][c];
            if (p == 0) continue;
            int absP = abs_int(p);
            bool isWhite = (p > 0);

            if (absP == queen) {
                if (isWhite) whiteQueens++; else blackQueens++;
                continue;
            }
            if (absP == king || absP == pawn) continue;

            if (absP == rook) {
                if (isWhite) { whiteRooks++; whiteOther++; } else { blackRooks++; blackOther++; }
            } else if (absP == knight || absP == bishop) {
                if (isWhite) { whiteMinors++; whiteOther++; } else { blackMinors++; blackOther++; }
            } else {
                if (isWhite) whiteOther++; else blackOther++;
            }
        }
    }

    if (whiteQueens == 0 && blackQueens == 0) return true;

    if (whiteQueens > 0) {
        if (whiteRooks > 0) return false;
        if (whiteOther > 1) return false;
        if (whiteOther == 1 && whiteMinors != 1) return false;
    }
    if (blackQueens > 0) {
        if (blackRooks > 0) return false;
        if (blackOther > 1) return false;
        if (blackOther == 1 && blackMinors != 1) return false;
    }

    return true;
}

int evaluate_board(const Board& board) {
    return evaluate_board_pesto(board);
}

int repetition_draw_score(const Board& board) {
    int currentScore = evaluate_board(board);
    if (!board.isWhiteTurn) currentScore = -currentScore;
    
    // if my position is clearly better, then a draw is bad
    if (currentScore > 100) return -50; 
    
    // if my position is clearly worse, then a draw is good
    return 0;
}