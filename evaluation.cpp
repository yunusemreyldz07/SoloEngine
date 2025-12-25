#include "evaluation.h"
#include <cmath>
#include "board.h"

constexpr int PIECE_VALUES[] = {0, 100, 350, 350, 525, 1000, 20000};

constexpr int pawn_pst[8][8] = {
    {0,  0,  0,  0,  0,  0,  0,  0},
    {50, 50, 50, 50, 50, 50, 50, 50},
    {10, 10, 20, 30, 30, 20, 10, 10},
    {5,  5, 10, 25, 25, 10,  5,  5},
    {0,  0,  0, 20, 20,  0,  0,  0},
    {5, -5,-10,  0,  0,-10, -5,  5},
    {5, 10, 10,-20,-20, 10, 10,  5},
    {0,  0,  0,  0,  0,  0,  0,  0}
};

constexpr int knight_pst[8][8] = {
    {-50,-40,-30,-30,-30,-30,-40,-50},
    {-40,-20,  0,  0,  0,  0,-20,-40},
    {-30,  0, 10, 15, 15, 10,  0,-30},
    {-30,  5, 15, 20, 20, 15,  5,-30},
    {-30,  0, 15, 20, 20, 15,  0,-30},
    {-30,  5, 10, 15, 15, 10,  5,-30},
    {-40,-20,  0,  5,  5,  0,-20,-40},
    {-50,-40,-30,-30,-30,-30,-40,-50}
};

constexpr int bishop_pst[8][8] = {
    {-20,-10,-10,-10,-10,-10,-10,-20},
    {-10,  0,  0,  0,  0,  0,  0,-10},
    {-10,  0,  5, 10, 10,  5,  0,-10},
    {-10,  5,  5, 10, 10,  5,  5,-10},
    {-10,  0, 10, 10, 10, 10,  0,-10},
    {-10, 10, 10, 10, 10, 10, 10,-10},
    {-10,  5,  0,  0,  0,  0,  5,-10},
    {-20,-10,-10,-10,-10,-10,-10,-20}
};

constexpr int rook_pst[8][8] = {
    {0,  0,  0,  0,  0,  0,  0,  0},
    {5, 10, 10, 10, 10, 10, 10,  5},
    {-5, 0, 0, 0, 0, 0, 0, -5},
    {-5, 0, 0, 0, 0, 0, 0, -5},
    {-5, 0, 0, 0, 0, 0, 0, -5},
    {-5, 0, 0, 0, 0, 0, 0, -5},
    {-5, 0, 0, 0, 0, 0, 0, -5},
    {0,  0,  0,  5,  5,  0,  0,  0}
};

constexpr int queen_pst[8][8] = {
    {-20,-10,-10, -5, -5,-10,-10,-20},
    {-10,  0,  0,  0,  0,  0,  0,-10},
    {-10,  0,  5,  5,  5,  5,  0,-10},
    { -5,  0,  5,  5,  5,  5,  0, -5},
    {  0,  0,  5,  5,  5,  5,  0, -5},
    {-10,  5,  5,  5,  5,  5,  0,-10},
    {-10,  0,  5,  0,  0,  0,  0,-10},
    {-20,-10,-10, -5, -5,-10,-10,-20}
};

constexpr int mg_king_pst[8][8] = {
    {-30,-40,-40,-50,-50,-40,-40,-30},
    {-30,-40,-40,-50,-50,-40,-40,-30},
    {-30,-40,-40,-50,-50,-40,-40,-30},
    {-30,-40,-40,-50,-50,-40,-40,-30},
    {-20,-30,-30,-40,-40,-30,-30,-20},
    {-10,-20,-20,-20,-20,-20,-20,-10},
    {20, 20,  0,  0,  0,  0, 20, 20},
    {20, 30, 10,  0,  0, 10, 30, 20}
};

constexpr int eg_king_pst[8][8] = {
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

int evaluate_board(const Board& board) {
    int score = 0;
    const bool endgame = is_endgame(board);
    int whitesBlackBishop = 0;
    int whitesWhiteBishop = 0;
    int blacksBlackBishop = 0;
    int blacksWhiteBishop = 0;

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
                {
                    score += (p > 0) ? pawn_pst[r][c] : -pawn_pst[pr][c];
                    bool isolated = true;
                    if (c > 0) {
                        for (int rr = 0; rr < 8; rr++) {
                            int leftPiece = board.squares[rr][c-1];
                            if (abs_int(leftPiece) == pawn && (leftPiece > 0) == (p > 0)) {
                                isolated = false;
                                break;
                            }
                        }
                    }
                    if (c < 7) {
                        for (int rr = 0; rr < 8; rr++) {
                            int rightPiece = board.squares[rr][c+1];
                            if (abs_int(rightPiece) == pawn && (rightPiece > 0) == (p > 0)) {
                                isolated = false;
                                break;
                            }
                        }
                    }
                    if (isolated) {
                        score += (p > 0) ? -15 : 15; // isolated pawn is bad
                    }

                    // Doubled pawns penalty
                    for (int rr = r+1; rr < 8; rr++) {
                        if (board.squares[rr][c] == p) {
                            score += (p > 0) ? -20 : 20; // Doubled pawns are bad
                            break;
                        }
                    }
                    bool passed = true;
                    int direction = (p > 0) ? -1 : 1;

                    // Opposing pawns ahead on adjacent files block passed-pawn status.
                    for (int checkCol = std::max(0, c - 1); checkCol <= std::min(7, c + 1); checkCol++) {
                        for (int rr = r + direction; rr >= 0 && rr < 8; rr += direction) {
                            int checkPiece = board.squares[rr][checkCol];
                            if (checkPiece == -p) { // opponent pawn
                                passed = false;
                                break;
                            }
                        }
                        if (!passed) break;
                    }

                    // If any piece blocks the path on this file, dampen the bonus.
                    bool path_blocked = false;
                    for (int rr = r + direction; rr >= 0 && rr < 8; rr += direction) {
                        if (board.squares[rr][c] != 0) {
                            path_blocked = true;
                            break;
                        }
                    }

                    if (passed) {
                        int rank = (p > 0) ? (7 - r) : r;
                        int bonus = rank * rank * 8; // softer growth to avoid over-valuing stuck pawns
                        if (path_blocked) {
                            bonus /= 4; // heavily discount if it cannot currently advance
                        }
                        score += (p > 0) ? bonus : -bonus;
                    }
                    break;
                }
                case knight:
                    score += (p > 0) ? knight_pst[r][c] : -knight_pst[pr][c];
                    break;
                case bishop:
                {
                    bool isLightSquare = ((r + c) % 2 == 0) ? true : false; 
                    if (p > 0) {
                        if (isLightSquare) whitesWhiteBishop++;
                        else whitesBlackBishop++;
                        score += bishop_pst[r][c];
                    } 
                    else {
                        if (isLightSquare) blacksWhiteBishop++;
                        else blacksBlackBishop++;
                        score -= bishop_pst[pr][c];
                    }
                    break;
                }
                case rook:
                {
                    score += (p > 0) ? rook_pst[r][c] : -rook_pst[pr][c];
                    bool openFile = true;
                    bool semiOpenFile = true;
                    for (int rr = 0; rr < 8; rr++) {
                        int pieceOnFile = board.squares[rr][c];
                        if (pieceOnFile != 0) {
                            int absPiece = abs_int(pieceOnFile);
                            if (absPiece == pawn) {
                                if ((pieceOnFile > 0) == (p > 0)) {
                                    // you got your own pawn on the file
                                    semiOpenFile = false;
                                    openFile = false;
                                } else {
                                    // opponent's pawn on the file
                                    openFile = false;
                                }
                            }
                        }
                    }
                    
                    if (openFile) {
                        score += (p > 0) ? 50 : -50; // Open file is very valuable
                    } else if (semiOpenFile) {
                        score += (p > 0) ? 25 : -25; // Semi open file is also good
                    }
                    break;
                }
                case queen:
                    score += (p > 0) ? queen_pst[r][c] : -queen_pst[pr][c];
                    break;
                case king:
                    {
                        if (endgame) {
                            score += (p > 0) ? eg_king_pst[r][c] : -eg_king_pst[pr][c];
                            int centerDist = abs_int(r - 3.5) + abs_int(c - 3.5);
                            score += (p > 0) ? (7 - centerDist) * 5 : -(7 - centerDist) * 5;
                        } else {
                            score += (p > 0) ? mg_king_pst[r][c] : -mg_king_pst[pr][c];
                        }
                        break;
                    }
            }
        }
    }

    // protect the pawns in front of the king
    if (!endgame) {
        int wKingFile = board.whiteKingCol;
        int pawnShield = 0;
        
        // count pawns in front of the white king
        for (int dc = -1; dc <= 1; dc++) {
            int c = wKingFile + dc;
            if (c < 0 || c >= 8) continue;
            
            // 2nd and 3rd ranks have pawns?
            if (board.squares[6][c] == pawn) pawnShield += 20; // Right in front
            else if (board.squares[5][c] == pawn) pawnShield += 10; // One square ahead
        }
        
        score += pawnShield;
        
        // Same for black king
        int bKingFile = board.blackKingCol;
        int blackPawnShield = 0;
        for (int dc = -1; dc <= 1; dc++) {
            int c = bKingFile + dc;
            if (c < 0 || c >= 8) continue;
            if (board.squares[1][c] == -pawn) blackPawnShield += 20;
            else if (board.squares[2][c] == -pawn) blackPawnShield += 10;
        }
        
        score -= blackPawnShield;
    }

    if (whitesWhiteBishop >= 1 && whitesBlackBishop >= 1) {
        score += is_endgame(board) ? 60 : 30;
    }
    if (blacksWhiteBishop >= 1 && blacksBlackBishop >= 1) {
        score -= is_endgame(board) ? 60 : 30;
    }

    return score;

    // Mobility bonus
    int whiteMobility = 0;
    int blackMobility = 0;

    for (int r = 0; r < 8; r++) {
        for (int c = 0; c < 8; c++) {
            int p = board.squares[r][c];
            if (p == 0) continue;
            
            int absPiece = abs_int(p);
            if (absPiece == pawn || absPiece == king) continue; // Skip pawns/king
            
            std::vector<Move> moves;
            switch (absPiece) {
                case knight: moves = generate_knight_moves(board, r, c); break;
                case bishop: moves = generate_bishop_moves(board, r, c); break;
                case rook: moves = generate_rook_moves(board, r, c); break;
                case queen: moves = generate_queen_moves(board, r, c); break;
            }
            
            if (p > 0) {
                whiteMobility += moves.size();
            } else {
                blackMobility += moves.size();
            }
        }
    }

    score += (whiteMobility - blackMobility) * 3; // every extra move is worth 3 centipawns
}

int repetition_draw_score(const Board& board) {
    int standPat = evaluate_board(board);
    if (standPat > 0) return standPat / 2;
    return 0;
}