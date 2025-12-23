#include "evaluation.h"
#include <cmath>
#include "board.h"

const int PIECE_VALUES[] = {0, 100, 350, 350, 525, 1000, 20000};

const int pawn_pst[8][8] = {
    {0,  0,  0,  0,  0,  0,  0,  0},
    {50, 50, 50, 50, 50, 50, 50, 50},
    {10, 10, 20, 30, 30, 20, 10, 10},
    {5,  5, 10, 25, 25, 10,  5,  5},
    {0,  0,  0, 20, 20,  0,  0,  0},
    {5, -5,-10,  0,  0,-10, -5,  5},
    {5, 10, 10,-20,-20, 10, 10,  5},
    {0,  0,  0,  0,  0,  0,  0,  0}
};

const int knight_pst[8][8] = {
    {-50,-40,-30,-30,-30,-30,-40,-50},
    {-40,-20,  0,  0,  0,  0,-20,-40},
    {-30,  0, 10, 15, 15, 10,  0,-30},
    {-30,  5, 15, 20, 20, 15,  5,-30},
    {-30,  0, 15, 20, 20, 15,  0,-30},
    {-30,  5, 10, 15, 15, 10,  5,-30},
    {-40,-20,  0,  5,  5,  0,-20,-40},
    {-50,-40,-30,-30,-30,-30,-40,-50}
};

const int bishop_pst[8][8] = {
    {-20,-10,-10,-10,-10,-10,-10,-20},
    {-10,  0,  0,  0,  0,  0,  0,-10},
    {-10,  0,  5, 10, 10,  5,  0,-10},
    {-10,  5,  5, 10, 10,  5,  5,-10},
    {-10,  0, 10, 10, 10, 10,  0,-10},
    {-10, 10, 10, 10, 10, 10, 10,-10},
    {-10,  5,  0,  0,  0,  0,  5,-10},
    {-20,-10,-10,-10,-10,-10,-10,-20}
};

const int rook_pst[8][8] = {
    {0,  0,  0,  0,  0,  0,  0,  0},
    {5, 10, 10, 10, 10, 10, 10,  5},
    {-5, 0, 0, 0, 0, 0, 0, -5},
    {-5, 0, 0, 0, 0, 0, 0, -5},
    {-5, 0, 0, 0, 0, 0, 0, -5},
    {-5, 0, 0, 0, 0, 0, 0, -5},
    {-5, 0, 0, 0, 0, 0, 0, -5},
    {0,  0,  0,  5,  5,  0,  0,  0}
};

const int queen_pst[8][8] = {
    {-20,-10,-10, -5, -5,-10,-10,-20},
    {-10,  0,  0,  0,  0,  0,  0,-10},
    {-10,  0,  5,  5,  5,  5,  0,-10},
    { -5,  0,  5,  5,  5,  5,  0, -5},
    {  0,  0,  5,  5,  5,  5,  0, -5},
    {-10,  5,  5,  5,  5,  5,  0,-10},
    {-10,  0,  5,  0,  0,  0,  0,-10},
    {-20,-10,-10, -5, -5,-10,-10,-20}
};

const int mg_king_pst[8][8] = {
    {-30,-40,-40,-50,-50,-40,-40,-30},
    {-30,-40,-40,-50,-50,-40,-40,-30},
    {-30,-40,-40,-50,-50,-40,-40,-30},
    {-30,-40,-40,-50,-50,-40,-40,-30},
    {-20,-30,-30,-40,-40,-30,-30,-20},
    {-10,-20,-20,-20,-20,-20,-20,-10},
    {20, 20,  0,  0,  0,  0, 20, 20},
    {20, 30, 10,  0,  0, 10, 30, 20}
};

const int eg_king_pst[8][8] = {
    {-50,-40,-30,-20,-20,-30,-40,-50},
    {-30,-20,-10,  0,  0,-10,-20,-30},
    {-30,-10, 20, 30, 30, 20,-10,-30},
    {-30,-10, 30, 40, 40, 30,-10,-30},
    {-30,-10, 30, 40, 40, 30,-10,-30},
    {-30,-10, 20, 30, 30, 20,-10,-30},
    {-30,-30,  0,  0,  0,  0,-30,-30},
    {-50,-30,-30,-30,-30,-30,-30,-50}
};

inline int abs_int(int x) {
    return x < 0 ? -x : x;
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

int evaulate_board(const Board& board) {
    int score = 0;
    const bool endgame = is_endgame(board);

    for (int r = 0; r < 8; r++) {
        for (int c = 0; c < 8; c++) {
            int p = board.squares[r][c];
            if (p == 0) continue;
            int absPiece = abs_int(p);
            int sign = (p > 0) ? 1 : -1;

            // Material
            score += sign * PIECE_VALUES[absPiece];

            // PST contribution — flip for black by using mirrored row
            int pr = (p > 0) ? r : 7 - r;
            switch (absPiece) {
                case pawn:
                    score += (p > 0) ? pawn_pst[r][c] : -pawn_pst[pr][c];
                    break;
                case knight:
                    score += (p > 0) ? knight_pst[r][c] : -knight_pst[pr][c];
                    break;
                case bishop:
                    score += (p > 0) ? bishop_pst[r][c] : -bishop_pst[pr][c];
                    break;
                case rook:
                    score += (p > 0) ? rook_pst[r][c] : -rook_pst[pr][c];
                    break;
                case queen:
                    score += (p > 0) ? queen_pst[r][c] : -queen_pst[pr][c];
                    break;
                case king:
                    if (endgame) score += (p > 0) ? eg_king_pst[r][c] : -eg_king_pst[pr][c];
                    else score += (p > 0) ? mg_king_pst[r][c] : -mg_king_pst[pr][c];
                    break;
                default:
                    break;
            }
        }
    }

    return score;
}

int repetition_draw_score(const Board& board) {
    int standPat = evaulate_board(board);
    const int contempt = 100;
    if (standPat > 200) return -contempt;
    if (standPat < -200) return contempt;
    return 0;
}