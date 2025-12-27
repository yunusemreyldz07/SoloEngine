#include "evaluation.h"
#include <cmath>
#include "board.h"

constexpr int PIECE_VALUES[] = {0, 100, 350, 350, 500, 1000, 20000};

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
    bool isOpening = false;
    bool isMidgame = false;
    int totalPieces = 0;

    for (int row = 0; row < 8; row++) {
        for (int column = 0; column < 8; column++) {
            if (board.squares[row][column] != 0) totalPieces++;
            isOpening = (totalPieces >= 28); // 28+ pieces = opening 
            isMidgame = (totalPieces >= 16 && totalPieces < 28);
            int p = board.squares[row][column];
            if (p == 0) continue;
            int absPiece = abs_int(p);
            int sign = (p > 0) ? 1 : -1;

            // Material
            score += sign * PIECE_VALUES[absPiece];

            // PST contribution — flip for black by using mirrored row
            int pr = (p > 0) ? row : 7 - row;
            switch (absPiece) {
                case pawn:
                {
                    score += (p > 0) ? pawn_pst[row][column] : -pawn_pst[pr][column];
                    bool isolated = true;
                    if (column > 0) {
                        for (int rr = 0; rr < 8; rr++) {
                            int leftPiece = board.squares[rr][column-1];
                            if (abs_int(leftPiece) == pawn && (leftPiece > 0) == (p > 0)) {
                                isolated = false;
                                break;
                            }
                        }
                    }
                    if (column < 7) {
                        for (int rr = 0; rr < 8; rr++) {
                            int rightPiece = board.squares[rr][column+1];
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
                    for (int rr = row+1; rr < 8; rr++) {
                        if (board.squares[rr][column] == p) {
                            score += (p > 0) ? -20 : 20; // Doubled pawns are bad
                            break;
                        }
                    }
                    bool passed = true;
                    int direction = (p > 0) ? -1 : 1;

                    // Opposing pawns ahead on adjacent files block passed-pawn status.
                    for (int checkCol = std::max(0, column - 1); checkCol <= std::min(7, column + 1); checkCol++) {
                        for (int rr = row + direction; rr >= 0 && rr < 8; rr += direction) {
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
                    for (int rr = row + direction; rr >= 0 && rr < 8; rr += direction) {
                        if (board.squares[rr][column] != 0) {
                            path_blocked = true;
                            break;
                        }
                    }

                    if (passed) {
                        int rank = (p > 0) ? (7 - row) : row;
                        int bonus = rank * rank * 8; // softer growth to avoid over-valuing stuck pawns
                        if (path_blocked) {
                            bonus /= 4; // heavily discount if it cannot currently advance
                        }
                        score += (p > 0) ? bonus : -bonus;
                    }
                    break;
                }
                case knight:
                    score += (p > 0) ? knight_pst[row][column] : -knight_pst[pr][column];
                    break;
                case bishop:
                {
                    bool isLightSquare = ((row + column) % 2 == 0) ? true : false; 
                    if (p > 0) {
                        if (isLightSquare) whitesWhiteBishop++;
                        else whitesBlackBishop++;
                        score += bishop_pst[row][column];
                    } 
                    else {
                        if (isLightSquare) blacksWhiteBishop++;
                        else blacksBlackBishop++;
                        score -= bishop_pst[pr][column];
                    }
                    break;
                }
                case rook:
                {
                    score += (p > 0) ? rook_pst[row][column] : -rook_pst[pr][column];
                    bool openFile = true;
                    bool semiOpenFile = true;
                    for (int rr = 0; rr < 8; rr++) {
                        int pieceOnFile = board.squares[rr][column];
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
                    // Connected rooks bonus
                    for (int r = 0; r < 8; r++) {
                        for (int c = 0; c < 8; c++) {
                            int p = board.squares[r][c];
                            if (abs_int(p) != rook) continue;
                            
                            // Any same file rook?
                            for (int rr = r + 1; rr < 8; rr++) {
                                int other = board.squares[rr][c];
                                if (other == p) { // Same color rook
                                    // Are there any pieces between them?
                                    bool connected = true;
                                    for (int between = r + 1; between < rr; between++) {
                                        if (board.squares[between][c] != 0) {
                                            connected = false;
                                            break;
                                        }
                                    }
                                    if (connected) {
                                        score += (p > 0) ? 30 : -30;
                                    }
                                }
                            }
                            
                            // Checking the same rank
                            for (int cc = c + 1; cc < 8; cc++) {
                                int other = board.squares[r][cc];
                                if (other == p) { // Same color rook
                                    // Are there any pieces between them?
                                    bool connected = true;
                                    for (int between = c + 1; between < cc; between++) {
                                        if (board.squares[r][between] != 0) {
                                            connected = false;
                                            break;
                                        }
                                    }
                                    if (connected) {
                                        score += (p > 0) ? 30 : -30;
                                    }
                                }
                            }

                        }
                    }

                    break;
                }
                case queen:
                    score += (p > 0) ? queen_pst[row][column] : -queen_pst[pr][column];
                    break;
                case king:
                    {
                        if (endgame) {
                            score += (p > 0) ? eg_king_pst[row][column] : -eg_king_pst[pr][column];
                            int centerDist = abs_int(row - 3.5) + abs_int(column - 3.5);
                            score += (p > 0) ? (7 - centerDist) * 5 : -(7 - centerDist) * 5;
                        } else {
                            score += (p > 0) ? mg_king_pst[row][column] : -mg_king_pst[pr][column]; // midgame PST
                            // protect the pawns in front of the king

                            int wKingFile = board.whiteKingCol;
                            int pawnShield = 0;
                            
                            // Count pawns in front of the king
                            for (int dc = -1; dc <= 1; dc++) {
                                int column = wKingFile + dc;
                                if (column < 0 || column >= 8) continue;
                                
                                // Are there pawns on the 2nd and 3rd ranks?
                                if (board.squares[6][column] == pawn) pawnShield += 20; // Right in front
                                else if (board.squares[5][column] == pawn) pawnShield += 10; // One square ahead
                            }
                            
                            score += pawnShield;
                            
                            // Same for black king
                            int bKingFile = board.blackKingCol;
                            int blackPawnShield = 0;
                            for (int dc = -1; dc <= 1; dc++) {
                                int column = bKingFile + dc;
                                if (column < 0 || column >= 8) continue;
                                if (board.squares[1][column] == -pawn) blackPawnShield += 20;
                                else if (board.squares[2][column] == -pawn) blackPawnShield += 10;
                            }
                            
                            score -= blackPawnShield;
                        }
                        break;
                    }
            }
        }
    }

    if (isOpening) {
        int whiteDevelopment = 0;
        int blackDevelopment = 0;
        
        // Knights should move from starting squares
        if (board.squares[7][1] == knight) whiteDevelopment -= 30; // still at b1
        if (board.squares[7][6] == knight) whiteDevelopment -= 30; // still at g1
        
        // Reward developed knights
        if (board.squares[5][2] == knight) whiteDevelopment += 40; // c3
        if (board.squares[5][5] == knight) whiteDevelopment += 40; // f3
        if (board.squares[6][2] == knight) whiteDevelopment += 25; // c2 (acceptable)
        if (board.squares[6][5] == knight) whiteDevelopment += 25; // f2 (acceptable)
        
        // Bishops should move from starting squares
        if (board.squares[7][2] == bishop) whiteDevelopment -= 25; // still at c1
        if (board.squares[7][5] == bishop) whiteDevelopment -= 25; // still at f1
        
        // Castling is great in opening
        if (!board.whiteCanCastleKingSide && !board.whiteCanCastleQueenSide) {
            // Already castled (king moved)
            if (board.whiteKingCol != 4) {
                whiteDevelopment += 50; // Castled
            }
        }
        
        // Queen shouldn't move early
        if (board.squares[7][3] != queen) { // Queen not on starting square
            bool queenMoved = false;
            for (int row = 0; row < 8; row++) {
                for (int column = 0; column < 8; column++) {
                    if (board.squares[row][column] == queen) {
                        queenMoved = true;
                        // Penalty for early queen moves
                        if (row != 7) whiteDevelopment -= 40;
                        break;
                    }
                }
                if (queenMoved) break;
            }
        }
        
        // BLACK DEVELOPMENT (mirror logic)
        if (board.squares[0][1] == -knight) blackDevelopment -= 30;
        if (board.squares[0][6] == -knight) blackDevelopment -= 30;
        
        if (board.squares[2][2] == -knight) blackDevelopment += 40; // c6
        if (board.squares[2][5] == -knight) blackDevelopment += 40; // f6
        if (board.squares[1][2] == -knight) blackDevelopment += 25;
        if (board.squares[1][5] == -knight) blackDevelopment += 25;
        
        if (board.squares[0][2] == -bishop) blackDevelopment -= 25;
        if (board.squares[0][5] == -bishop) blackDevelopment -= 25;
        
        if (!board.blackCanCastleKingSide && !board.blackCanCastleQueenSide) {
            if (board.blackKingCol != 4) {
                blackDevelopment += 50;
            }
        }
        
        if (board.squares[0][3] != -queen) {
            bool queenMoved = false;
            for (int row = 0; row < 8; row++) {
                for (int column = 0; column < 8; column++) {
                    if (board.squares[row][column] == -queen) {
                        queenMoved = true;
                        if (row != 0) blackDevelopment -= 40;
                        break;
                    }
                }
                if (queenMoved) break;
            }
        }
        
        score += whiteDevelopment - blackDevelopment;
    }


    // protect the pawns in front of the king
    if (isMidgame || isOpening) {
        int wKingFile = board.whiteKingCol;
        int pawnShield = 0;
        
        // count pawns in front of the white king
        for (int dc = -1; dc <= 1; dc++) {
            int column = wKingFile + dc;
            if (column < 0 || column >= 8) continue;
            
            // 2nd and 3rd ranks have pawns?
            if (board.squares[6][column] == pawn) pawnShield += 20; // Right in front
            else if (board.squares[5][column] == pawn) pawnShield += 10; // One square ahead
        }
        
        score += pawnShield;
        
        // Same for black king
        int bKingFile = board.blackKingCol;
        int blackPawnShield = 0;
        for (int dc = -1; dc <= 1; dc++) {
            int column = bKingFile + dc;
            if (column < 0 || column >= 8) continue;
            if (board.squares[1][column] == -pawn) blackPawnShield += 20;
            else if (board.squares[2][column] == -pawn) blackPawnShield += 10;
        }
        
        score -= blackPawnShield;
    }

    if (whitesWhiteBishop >= 1 && whitesBlackBishop >= 1) {
        score += is_endgame(board) ? 60 : 30;
    }
    if (blacksWhiteBishop >= 1 && blacksBlackBishop >= 1) {
        score -= is_endgame(board) ? 60 : 30;
    }

    // Mobility bonus
    int whiteMobility = 0;
    int blackMobility = 0;

    for (int row = 0; row < 8; row++) {
        for (int column = 0; column < 8; column++) {
            int p = board.squares[row][column];
            if (p == 0) continue;
            
            int absPiece = abs_int(p);
            if (absPiece == pawn || absPiece == king) continue; // Skip pawns/king
            
            std::vector<Move> moves;
            switch (absPiece) {
                case knight: moves = generate_knight_moves(board, row, column); break;
                case bishop: moves = generate_bishop_moves(board, row, column); break;
                case rook: moves = generate_rook_moves(board, row, column); break;
                case queen: moves = generate_queen_moves(board, row, column); break;
            }
            
            if (p > 0) {
                whiteMobility += moves.size();
            } else {
                blackMobility += moves.size();
            }
        }
    }

    score += (whiteMobility - blackMobility) * 3; // every extra move is worth 3 centipawns

    if (!is_endgame(board)) { // center control in opening/midgame
        int whiteCenterPawns = 0;
        int blackCenterPawns = 0;
        
        // Direct center pawn occupation
        if (board.squares[4][4] == pawn) whiteCenterPawns += 50; // e4
        if (board.squares[4][3] == pawn) whiteCenterPawns += 50; // d4
        if (board.squares[3][4] == -pawn) blackCenterPawns += 50; // e5
        if (board.squares[3][3] == -pawn) blackCenterPawns += 50; // d5
        
        // Extended center (c4, c5, f4, f5)
        if (board.squares[4][2] == pawn) whiteCenterPawns += 25; // c4
        if (board.squares[4][5] == pawn) whiteCenterPawns += 25; // f4
        if (board.squares[3][2] == -pawn) blackCenterPawns += 25; // c5
        if (board.squares[3][5] == -pawn) blackCenterPawns += 25; // f5
        
        score += whiteCenterPawns - blackCenterPawns;
        
        // Attacks on the center
        int whiteActivity = 0;
        int blackActivity = 0;
        
        // Bonus for pieces attacking center squares
        int centerSquares[4][2] = {{4,4}, {4,3}, {3,4}, {3,3}}; // e4, d4, e5, d5
        
        for (auto& square : centerSquares) {
            int row = square[0];
            int column = square[1];
            
            // Check if white attacks this square
            if (is_square_attacked(board, row, column, true)) {
                whiteActivity += 15;
            }
            // Check if black attacks this square
            if (is_square_attacked(board, row, column, false)) {
                blackActivity += 15;
            }
        }
        
        score += whiteActivity - blackActivity;
    }
    return score;
}

int repetition_draw_score(const Board& board) {
    int standPat = evaluate_board(board);
    if (standPat > 0) return standPat / 2;
    return 0;
}