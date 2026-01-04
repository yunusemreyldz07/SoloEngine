#include "board.h"
#include <iostream>
#include <algorithm>
#include <cmath>
#include <sstream>
// Piece encoding (constants declared in board.h)
char columns[] = "abcdefgh";

int chess_board[8][8] = {
    {-rook, -knight, -bishop, -queen, -king, -bishop, -knight, -rook},
    {-pawn, -pawn, -pawn, -pawn, -pawn, -pawn, -pawn, -pawn},
    {empty_sqr, empty_sqr, empty_sqr, empty_sqr, empty_sqr, empty_sqr, empty_sqr, empty_sqr},
    {empty_sqr, empty_sqr, empty_sqr, empty_sqr, empty_sqr, empty_sqr, empty_sqr, empty_sqr},
    {empty_sqr, empty_sqr, empty_sqr, empty_sqr, empty_sqr, empty_sqr, empty_sqr, empty_sqr},
    {empty_sqr, empty_sqr, empty_sqr, empty_sqr, empty_sqr, empty_sqr, empty_sqr, empty_sqr},
    {pawn, pawn, pawn, pawn, pawn, pawn, pawn, pawn},
    {rook, knight, bishop, queen, king, bishop, knight, rook}
};

int knight_moves[8][2] = {
    {-2, -1}, {-2, 1}, {-1, -2}, {-1, 2},
    {1, -2}, {1, 2}, {2, -1}, {2, 1}
};

int king_moves[8][2] = {
    {-1, -1}, {-1, 0}, {-1, 1},
    {0, -1},          {0, 1},
    {1, -1},  {1, 0}, {1, 1}
};

int bishop_directions[4][2] = {
    {-1, -1}, {-1, 1},
    {1, -1},  {1, 1}
};

int rook_directions[4][2] = {
    {-1, 0}, {1, 0},
    {0, -1}, {0, 1}
};

int queen_directions[8][2] = {
    {-1, -1}, {-1, 0}, {-1, 1},
    {0, -1},          {0, 1},
    {1, -1},  {1, 0}, {1, 1}
};

inline char to_upper_ascii(char c) {
    return (c >= 'a' && c <= 'z') ? static_cast<char>(c - 'a' + 'A') : c;
}

// Move struct constructor
Move::Move()
    : fromRow(0), fromCol(0), toRow(0), toCol(0), capturedPiece(0), promotion(0),
      prevW_KingSide(false), prevW_QueenSide(false), prevB_KingSide(false), prevB_QueenSide(false),
      prevEnPassantCol(-1), isEnPassant(false), isCastling(false) {}

// Board constructor
Board::Board() {
    resetBoard();
}

void Board::resetBoard() {
    for (int i = 0; i < 8; ++i) {
        for (int j = 0; j < 8; ++j) {
            squares[i][j] = chess_board[i][j];
        }
    }

    whiteCanCastleKingSide = true;
    whiteCanCastleQueenSide = true;
    blackCanCastleKingSide = true;
    blackCanCastleQueenSide = true;
    isWhiteTurn = true;
    enPassantCol = -1;
    whiteKingRow = 7; whiteKingCol = 4;
    blackKingRow = 0; blackKingCol = 4;
}

void Board::makeMove(Move& move) {
    move.prevW_KingSide = whiteCanCastleKingSide;
    move.prevW_QueenSide = whiteCanCastleQueenSide;
    move.prevB_KingSide = blackCanCastleKingSide;
    move.prevB_QueenSide = blackCanCastleQueenSide;
    move.prevEnPassantCol = enPassantCol;

    move.isEnPassant = false;
    move.isCastling = false;

    int piece = squares[move.fromRow][move.fromCol];
    int target_square = squares[move.toRow][move.toCol];
    move.capturedPiece = target_square;
    squares[move.fromRow][move.fromCol] = 0;
    squares[move.toRow][move.toCol] = piece;

    if (abs(piece) == 1 && move.fromCol != move.toCol && move.capturedPiece == 0) {
        move.isEnPassant = true;
        int captureRow = move.fromRow;
        squares[captureRow][move.toCol] = 0;
    }
    
    if (abs(piece) == 6 && abs(move.fromCol - move.toCol) == 2) {
        if (move.toCol > move.fromCol) {
            // king-side
            squares[move.toRow][move.toCol - 1] = squares[move.toRow][move.toCol + 1];
            squares[move.toRow][move.toCol + 1] = 0;
        } else {
            // queen-side
            squares[move.toRow][move.toCol + 1] = squares[move.toRow][move.toCol - 2];
            squares[move.toRow][move.toCol - 2] = 0;
        }
        move.isCastling = true;
    }

    // Promotion
    if (abs(piece) == 1) {
        int promotionRank = piece > 0 ? 0 : 7;
        if (move.toRow == promotionRank && move.promotion != 0) {
            squares[move.toRow][move.toCol] = (piece > 0) ? move.promotion : -move.promotion;
        }
    }

    if (abs(piece) == pawn && abs(move.fromRow - move.toRow) == 2) {
        // En-passant is only relevant if the opponent can actually capture.
        // This affects repetition detection and hashing.
        const int enemyPawn = (piece > 0) ? -pawn : pawn;
        bool canCapture = false;
        for (int dc : {-1, 1}) {
            const int c = move.toCol + dc;
            if (c >= 0 && c < 8) {
                if (squares[move.toRow][c] == enemyPawn) {
                    canCapture = true;
                    break;
                }
            }
        }
        enPassantCol = canCapture ? move.toCol : -1;
    } else {
        enPassantCol = -1;
    }

    isWhiteTurn = !isWhiteTurn;

    if (piece == 4 && move.fromRow == 7 && move.fromCol == 7){
        whiteCanCastleKingSide = false;
    }
    
    else if (piece == 4 && move.fromRow == 7 && move.fromCol == 0){
        whiteCanCastleQueenSide = false;
    }
    else if (piece == -4 && move.fromRow == 0 && move.fromCol == 7){
        blackCanCastleKingSide = false;
    }
    else if (piece == -4 && move.fromRow == 0 && move.fromCol == 0){
        blackCanCastleQueenSide = false;
    }

    if (piece == 6) {
        whiteCanCastleKingSide = false;
        whiteCanCastleQueenSide = false;
    }
    if (piece == -6) {
        blackCanCastleKingSide = false;
        blackCanCastleQueenSide = false;
    }

    if (move.capturedPiece == rook && move.toRow == 7 && move.toCol == 7) {
        whiteCanCastleKingSide = false;
    }
    if (move.capturedPiece == rook && move.toRow == 7 && move.toCol == 0) {
        whiteCanCastleQueenSide = false;
    }
    if (move.capturedPiece == -rook && move.toRow == 0 && move.toCol == 7) {
        blackCanCastleKingSide = false;
    }
    if (move.capturedPiece == -rook && move.toRow == 0 && move.toCol == 0) {
        blackCanCastleQueenSide = false;
    }
    if (piece == 6) {
        whiteKingRow = move.toRow;
        whiteKingCol = move.toCol;
    }
    if (piece == -6) {
        blackKingRow = move.toRow;
        blackKingCol = move.toCol;
    }
}

void Board::unmakeMove(Move& move) {
    isWhiteTurn = !isWhiteTurn;
    int piece = squares[move.toRow][move.toCol];
    if (move.promotion != 0) {
        piece = (piece > 0) ? pawn : -pawn;
    }
    
    squares[move.fromRow][move.fromCol] = piece;
    squares[move.toRow][move.toCol] = 0;

    if (move.isEnPassant) {
        int capturedPawn = isWhiteTurn ? -1 : 1;
        squares[move.fromRow][move.toCol] = capturedPawn;
    } else {
        squares[move.toRow][move.toCol] = move.capturedPiece;
    }

    if (move.isCastling) {
        if (move.toCol > move.fromCol) {
            // king-side
            squares[move.toRow][move.toCol + 1] = squares[move.toRow][move.toCol - 1];
            squares[move.toRow][move.toCol - 1] = 0;
        } else {
            // queen-side
            squares[move.toRow][move.toCol - 2] = squares[move.toRow][move.toCol + 1];
            squares[move.toRow][move.toCol + 1] = 0;
        }
    }

    whiteCanCastleKingSide = move.prevW_KingSide;
    whiteCanCastleQueenSide = move.prevW_QueenSide;
    blackCanCastleKingSide = move.prevB_KingSide;
    blackCanCastleQueenSide = move.prevB_QueenSide;
    enPassantCol = move.prevEnPassantCol;
    if (piece == 6) {
        whiteKingRow = move.fromRow;
        whiteKingCol = move.fromCol;
    }
    if (piece == -6) {
        blackKingRow = move.fromRow;
        blackKingCol = move.fromCol;
    }
}

void Board::loadFromFEN(const std::string& fen) {
    // clear board
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            squares[i][j] = 0;
        }
    }
    
    // example fen "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"
    std::istringstream ss(fen);
    std::string position, turn, castling, enPassant;
    ss >> position >> turn >> castling >> enPassant;
    
    // placing pieces
    int row = 0, col = 0;
    for (char c : position) {
        if (c == '/') {
            row++;
            col = 0;
        } else if (c >= '1' && c <= '8') {
            col += (c - '0'); // empty squares
        } else {
            int piece = 0;
            switch (std::tolower(c)) {
                case 'p': piece = pawn; break;
                case 'n': piece = knight; break;
                case 'b': piece = bishop; break;
                case 'r': piece = rook; break;
                case 'q': piece = queen; break;
                case 'k': piece = king; break;
            }
            if (std::isupper(c)) {
                squares[row][col] = piece; // white
            } else {
                squares[row][col] = -piece; // black
            }
            
            // Find king positions
            if (piece == king) {
                if (std::isupper(c)) {
                    whiteKingRow = row;
                    whiteKingCol = col;
                } else {
                    blackKingRow = row;
                    blackKingCol = col;
                }
            }
            col++;
        }
    }
    
    // Turn
    isWhiteTurn = (turn == "w");
    
    // Castling rights
    whiteCanCastleKingSide = (castling.find('K') != std::string::npos);
    whiteCanCastleQueenSide = (castling.find('Q') != std::string::npos);
    blackCanCastleKingSide = (castling.find('k') != std::string::npos);
    blackCanCastleQueenSide = (castling.find('q') != std::string::npos);
    
    // En passant
    if (enPassant != "-") {
        enPassantCol = enPassant[0] - 'a';

        // Normalize: only keep EP if side-to-move can capture it.
        const int pawnRow = isWhiteTurn ? 3 : 4; // rank 5 for white, rank 4 for black
        const int myPawn = isWhiteTurn ? pawn : -pawn;
        bool canCapture = false;
        if (enPassantCol - 1 >= 0 && squares[pawnRow][enPassantCol - 1] == myPawn) canCapture = true;
        if (enPassantCol + 1 < 8 && squares[pawnRow][enPassantCol + 1] == myPawn) canCapture = true;
        if (!canCapture) enPassantCol = -1;
    } else {
        enPassantCol = -1;
    }
}

void generate_pawn_moves(const Board& board, int row, int col, std::vector<Move>& moveList) {
    int piece = board.squares[row][col];
    int color = (piece > 0) ? 1 : -1; // 1 for white, -1 for black
    int direction = (color == 1) ? -1 : 1; // White moves up (-1), Black moves down (+1)
    int promotionRank = (color == 1) ? 0 : 7;

    // Single move forward
    int r = row + direction;
    if (r >= 0 && r < 8 && board.squares[r][col] == 0) {
        Move move;
        move.fromRow = row;
        move.fromCol = col;
        move.toRow = r;
        move.toCol = col;

        if (r == promotionRank) {
            for (int promoType : {queen, rook, bishop, knight}) {
                Move pm = move;
                pm.promotion = promoType;
                moveList.push_back(pm);
            }
        } else {
            moveList.push_back(move);

            // Double move forward from starting position
            if ((color == 1 && row == 6) || (color == -1 && row == 1)) {
                int r2 = row + 2 * direction;
                if (board.squares[r2][col] == 0) {
                    Move dm = move;
                    dm.toRow = r2;
                    moveList.push_back(dm);
                }
            }
        }
    }

    // Captures
    r = row + direction;
    if (r >= 0 && r < 8) {
        for (int dc = -1; dc <= 1; dc += 2) {
            int c = col + dc;
            if (c >= 0 && c < 8) {
                int target = board.squares[r][c];
                if (target != 0 && ((target > 0) != (piece > 0))) {
                    Move m;
                    m.fromRow = row; m.fromCol = col; m.toRow = r; m.toCol = c;
                    m.capturedPiece = target;
                    if (r == promotionRank) {
                        for (int promoType : {queen, rook, bishop, knight}) {
                            Move pm = m;
                            pm.promotion = promoType;
                            moveList.push_back(pm);
                        }
                    } else {
                        moveList.push_back(m);
                    }
                }
            }
        }
    }

    // En passant (independent of forward square being empty)
    if (board.enPassantCol != -1 && abs(col - board.enPassantCol) == 1) {
        int epTargetRow = row + direction;
        if (epTargetRow >= 0 && epTargetRow < 8) {
            int epPawnRow = row; // captured pawn sits next to us on current row
            int epPawnCol = board.enPassantCol;
            int expectedEnemyPawn = (color == 1) ? -pawn : pawn;
            if (board.squares[epTargetRow][epPawnCol] == 0 && board.squares[epPawnRow][epPawnCol] == expectedEnemyPawn) {
                Move m;
                m.fromRow = row; m.fromCol = col; m.toRow = epTargetRow; m.toCol = epPawnCol;
                m.isEnPassant = true;
                m.capturedPiece = expectedEnemyPawn;
                moveList.push_back(m);
            }
        }
    }
}

void generate_knight_moves(const Board& board, int row, int col, std::vector<Move>& moveList) {
    int piece = board.squares[row][col];
    int color = (piece > 0) ? 1 : -1; // 1 for white, -1 for black

    for (int i = 0; i < 8; ++i) {
        int r = row + knight_moves[i][0];
        int c = col + knight_moves[i][1];
        if (r >= 0 && r < 8 && c >= 0 && c < 8) {
            int target = board.squares[r][c];
            if (target == 0 || (target > 0 && color == -1) || (target < 0 && color == 1)) {
                Move m;
                m.fromRow = row; m.fromCol = col; m.toRow = r; m.toCol = c;
                m.capturedPiece = target;
                moveList.push_back(m);
            }
        }
    }
}

void generate_queen_moves(const Board& board, int row, int col, std::vector<Move>& moveList) {
    generate_bishop_moves(board, row, col, moveList);
    generate_rook_moves(board, row, col, moveList);
}

void generate_king_moves(const Board& board, int row, int col, std::vector<Move>& moveList) {
    int piece = board.squares[row][col];
    int color = (piece > 0) ? 1 : -1;

    for (int i = 0; i < 8; ++i) {
        int r = row + king_moves[i][0];
        int c = col + king_moves[i][1];
        if (r >= 0 && r < 8 && c >= 0 && c < 8) {
            int target = board.squares[r][c];
            if (target == 0 || (target > 0 && color == -1) || (target < 0 && color == 1)) {
                Move m; m.fromRow = row; m.fromCol = col; m.toRow = r; m.toCol = c; m.capturedPiece = target; moveList.push_back(m);
            }
        }
    }

    const bool opponentIsWhite = (color == -1);
    const bool kingInCheck = is_square_attacked(board, row, col, opponentIsWhite);

    if (!kingInCheck) {
        // Black castles
        if (color == -1 && row == 0 && col == 4) {
            if (board.blackCanCastleKingSide) {
                if (board.squares[0][5] == 0 && board.squares[0][6] == 0 && board.squares[0][7] == -rook) {
                    if (!is_square_attacked(board, 0, 5, true) && !is_square_attacked(board, 0, 6, true)) {
                        Move m; m.fromRow = row; m.fromCol = col; m.toRow = 0; m.toCol = 6; m.isCastling = true; moveList.push_back(m);
                    }
                }
            }
            if (board.blackCanCastleQueenSide) {
                if (board.squares[0][1] == 0 && board.squares[0][2] == 0 && board.squares[0][3] == 0 && board.squares[0][0] == -rook) {
                    if (!is_square_attacked(board, 0, 3, true) && !is_square_attacked(board, 0, 2, true)) {
                        Move m; m.fromRow = row; m.fromCol = col; m.toRow = 0; m.toCol = 2; m.isCastling = true; moveList.push_back(m);
                    }
                }
            }
        }

        // White castles
        if (color == 1 && row == 7 && col == 4) {
            if (board.whiteCanCastleKingSide) {
                if (board.squares[7][5] == 0 && board.squares[7][6] == 0 && board.squares[7][7] == rook) {
                    if (!is_square_attacked(board, 7, 5, false) && !is_square_attacked(board, 7, 6, false)) {
                        Move m; m.fromRow = row; m.fromCol = col; m.toRow = 7; m.toCol = 6; m.isCastling = true; moveList.push_back(m);
                    }
                }
            }
            if (board.whiteCanCastleQueenSide) {
                if (board.squares[7][1] == 0 && board.squares[7][2] == 0 && board.squares[7][3] == 0 && board.squares[7][0] == rook) {
                    if (!is_square_attacked(board, 7, 3, false) && !is_square_attacked(board, 7, 2, false)) {
                        Move m; m.fromRow = row; m.fromCol = col; m.toRow = 7; m.toCol = 2; m.isCastling = true; moveList.push_back(m);
                    }
                }
            }
        }
    }
}

void generate_bishop_moves(const Board& board, int row, int col, std::vector<Move>& moveList) {
    int piece = board.squares[row][col];
    int color = (piece > 0) ? 1 : -1; // 1 for white, -1 for black
    for (int d = 0; d < 4; ++d) {
        int r = row + bishop_directions[d][0];
        int c = col + bishop_directions[d][1];
        while (r >= 0 && r < 8 && c >= 0 && c < 8) {
            int target = board.squares[r][c];
            if (target == 0) {
                Move m; m.fromRow = row; m.fromCol = col; m.toRow = r; m.toCol = c; m.capturedPiece = 0; moveList.push_back(m);
            } else {
                if ((target > 0 && color == -1) || (target < 0 && color == 1)) {
                    Move m; m.fromRow = row; m.fromCol = col; m.toRow = r; m.toCol = c; m.capturedPiece = target; moveList.push_back(m);
                }
                break;
            }
            r += bishop_directions[d][0];
            c += bishop_directions[d][1];
        }
    }
}

void generate_rook_moves(const Board& board, int row, int col, std::vector<Move>& moveList) {
    int piece = board.squares[row][col];
    int color = (piece > 0) ? 1 : -1; // 1 for white, -1 for black
    for (int d = 0; d < 4; ++d) {
        int r = row + rook_directions[d][0];
        int c = col + rook_directions[d][1];
        while (r >= 0 && r < 8 && c >= 0 && c < 8) {
            int target = board.squares[r][c];
            if (target == 0) {
                Move m; m.fromRow = row; m.fromCol = col; m.toRow = r; m.toCol = c; m.capturedPiece = 0; moveList.push_back(m);
            } else {
                if ((target > 0 && color == -1) || (target < 0 && color == 1)) {
                    Move m; m.fromRow = row; m.fromCol = col; m.toRow = r; m.toCol = c; m.capturedPiece = target; moveList.push_back(m);
                }
                break;
            }
            r += rook_directions[d][0];
            c += rook_directions[d][1];
        }
    }
}

std::vector<Move> get_all_moves(Board& board, bool isWhiteTurn) {
    std::vector<Move> pseudoMoves;
    std::vector<Move> legalMoves;
    pseudoMoves.reserve(256);
    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 8; ++c) {
            int piece = board.squares[r][c];
            if (piece == 0) continue;
            if (isWhiteTurn && piece < 0) continue;
            if (!isWhiteTurn && piece > 0) continue;
            switch (abs(piece)) {
                case pawn:   generate_pawn_moves(board, r, c, pseudoMoves); break;
                case knight: generate_knight_moves(board, r, c, pseudoMoves); break;
                case bishop: generate_bishop_moves(board, r, c, pseudoMoves); break;
                case rook:   generate_rook_moves(board, r, c, pseudoMoves); break;
                case queen:  generate_queen_moves(board, r, c, pseudoMoves); break;
                case king:   generate_king_moves(board, r, c, pseudoMoves); break;
            }
        }
    }

    for (auto &m : pseudoMoves) {
    
        board.makeMove(m);
        int kingRow = isWhiteTurn ? board.whiteKingRow : board.blackKingRow;
        int kingCol = isWhiteTurn ? board.whiteKingCol : board.blackKingCol;
        if (is_square_attacked(board, kingRow, kingCol, !isWhiteTurn)) {
            board.unmakeMove(m);
            continue;
        }
        legalMoves.push_back(m);
        board.unmakeMove(m);
    }

    return legalMoves;
}

std::vector<Move> get_capture_moves(const Board& board) {
    std::vector<Move> moves;
    bool whiteToMove = board.isWhiteTurn;

    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            int piece = board.squares[row][col];
            if (piece == 0) continue;
            if (whiteToMove && piece < 0) continue;
            if (!whiteToMove && piece > 0) continue;
            int absP = abs(piece);
            std::vector<Move> gen;
            switch (absP) {
                case pawn: generate_pawn_moves(board, row, col, gen); break;
                case knight: generate_knight_moves(board, row, col, gen); break;
                case bishop: generate_bishop_moves(board, row, col, gen); break;
                case rook: generate_rook_moves(board, row, col, gen); break;
                case queen: generate_queen_moves(board, row, col, gen); break;
                case king: generate_king_moves(board, row, col, gen); break;
            }
            for (auto &m : gen) {
                if (m.capturedPiece != 0 || m.isEnPassant) moves.push_back(m);
            }
        }
    }
    return moves;
}

bool is_square_attacked(const Board& board, int row, int col, bool isWhiteAttacker) {
    // White pawns attack one rank "up" the board (row - 1), black pawns one rank "down" (row + 1).
    // The old code used the opposite sign, which let the engine think diagonals were safe when they
    // were in fact covered by pawns. This broke check detection and castling legality.
    int pawnDir = isWhiteAttacker ? 1 : -1;
    
    // Pawn attacks
    if (row + pawnDir >= 0 && row + pawnDir < 8) {
        if (col - 1 >= 0) {
            int p = board.squares[row + pawnDir][col - 1];
            if (p == (isWhiteAttacker ? 1 : -1)) return true;
        }
        if (col + 1 < 8) {
            int p = board.squares[row + pawnDir][col + 1];
            if (p == (isWhiteAttacker ? 1 : -1)) return true;
        }
    }

    // Knight
    for (int i = 0; i < 8; i++) {
        int r = row + knight_moves[i][0];
        int c = col + knight_moves[i][1];
        if (r >= 0 && r < 8 && c >= 0 && c < 8) {
            int p = board.squares[r][c];
            if (p == (isWhiteAttacker ? 2 : -2)) return true;
        }
    }

    // Queen and Rook
    for (int i = 0; i < 4; i++) {
        for (int dist = 1; dist < 8; dist++) {
            int r = row + rook_directions[i][0] * dist;
            int c = col + rook_directions[i][1] * dist;
            if (r < 0 || r >= 8 || c < 0 || c >= 8) break;
            int p = board.squares[r][c];
            if (p != 0) {
                if ((p > 0) == isWhiteAttacker && (abs(p) == queen || abs(p) == rook)) return true;
                break;
            }
        }
    }

    // Queen and Bishop
    for (int i = 0; i < 4; i++) {
        for (int dist = 1; dist < 8; dist++) {
            int r = row + bishop_directions[i][0] * dist;
            int c = col + bishop_directions[i][1] * dist;
            if (r < 0 || r >= 8 || c < 0 || c >= 8) break;
            int p = board.squares[r][c];
            if (p != 0) {
                if ((p > 0) == isWhiteAttacker && (abs(p) == queen || abs(p) == bishop)) return true;
                break;
            }
        }
    }

    // King
    for (int i = 0; i < 8; i++) {
        int r = row + king_moves[i][0];
        int c = col + king_moves[i][1];
        if (r >= 0 && r < 8 && c >= 0 && c < 8) {
            int p = board.squares[r][c];
            if (p == (isWhiteAttacker ? 6 : -6)) return true;
        }
    }

    return false;
}

void printBoard(const Board& board) {
    std::cout << "  +-----------------+" << std::endl;
    for (int r = 0; r < 8; r++) {
        std::cout << 8 - r << " | ";
        for (int c = 0; c < 8; c++) {
            int p = board.squares[r][c];
            char cStr = '.';
            if (p != 0) {
                char ch = ' ';
                int ap = abs(p);
                switch (ap) {
                    case pawn: ch = 'p'; break;
                    case knight: ch = 'n'; break;
                    case bishop: ch = 'b'; break;
                    case rook: ch = 'r'; break;
                    case queen: ch = 'q'; break;
                    case king: ch = 'k'; break;
                }
                if (p > 0) ch = to_upper_ascii(ch);
                cStr = ch;
            }
            std::cout << cStr << " ";
        }
        std::cout << "|" << std::endl;
    }
    std::cout << "  +-----------------+" << std::endl;
    std::cout << "    a b c d e f g h" << std::endl;
}

Move uci_to_move(const std::string& uci) {
    Move move;
    move.fromCol = uci[0] - 'a';
    move.fromRow = 8 - (uci[1] - '0');
    move.toCol = uci[2] - 'a';
    move.toRow = 8 - (uci[3] - '0');
    if (uci.length() == 5) {
        char promoChar = uci[4];
        switch (promoChar) {
            case 'q': move.promotion = queen; break;
            case 'r': move.promotion = rook; break;
            case 'b': move.promotion = bishop; break;
            case 'n': move.promotion = knight; break;
        }
    }
    return move;
}

// Zobrist implementation
Zobrist::Zobrist() {
    uint64_t seed = 0xC0FFEE1234ABCDEFULL;
    for (int p = 0; p < 12; p++) {
        for (int sq = 0; sq < 64; sq++) {
            piece[p][sq] = splitmix64(seed);
        }
    }
    for (int i = 0; i < 16; i++) castling[i] = splitmix64(seed);
    for (int i = 0; i < 9; i++) epFile[i] = splitmix64(seed);
    side = splitmix64(seed);
}

uint64_t Zobrist::splitmix64(uint64_t& x) {
    uint64_t z = (x += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

const Zobrist& zobrist() {
    static Zobrist z;
    return z;
}

int piece_to_zobrist_index(int piece) {
    int absP = abs(piece);
    if (absP < 1 || absP > 6) return -1;
    int base = (piece > 0) ? 0 : 6;
    return base + (absP - 1);
}

uint64_t position_key(const Board& board) {
    const Zobrist& z = zobrist();
    uint64_t h = 0;

    for (int r = 0; r < 8; r++) {
        for (int c = 0; c < 8; c++) {
            int p = board.squares[r][c];
            if (p == 0) continue;
            int idx = piece_to_zobrist_index(p);
            if (idx < 0) continue;
            int sq = r * 8 + c;
            h ^= z.piece[idx][sq];
        }
    }

    int castle = 0;
    if (board.whiteCanCastleKingSide) castle |= 1;
    if (board.whiteCanCastleQueenSide) castle |= 2;
    if (board.blackCanCastleKingSide) castle |= 4;
    if (board.blackCanCastleQueenSide) castle |= 8;
    h ^= z.castling[castle];

    int ep = (board.enPassantCol >= 0 && board.enPassantCol < 8) ? board.enPassantCol : 8;
    h ^= z.epFile[ep];

    if (board.isWhiteTurn) h ^= z.side;
    return h;
}

// Transposition Table (lockless, thread-safe)
TranspositionTable globalTT;

uint16_t TranspositionTable::packMove(const Move& m) {
    int from = m.fromRow * 8 + m.fromCol;
    int to = m.toRow * 8 + m.toCol;

    if (from < 0 || from >= 64) from = 0;
    if (to < 0 || to >= 64) to = 0;

    int promo = std::abs(m.promotion);
    if (promo < 0) promo = 0;
    if (promo > 7) promo = 7;

    return static_cast<uint16_t>((from & 63) | ((to & 63) << 6) | ((promo & 7) << 12));
}

Move TranspositionTable::unpackMove(uint16_t packed) {
    Move m;
    const int from = packed & 63;
    const int to = (packed >> 6) & 63;
    const int promo = (packed >> 12) & 7;

    m.fromRow = from / 8;
    m.fromCol = from % 8;
    m.toRow = to / 8;
    m.toCol = to % 8;
    m.promotion = promo;
    return m;
}

uint64_t TranspositionTable::packData(int score, int depth, TTFlag flag, uint16_t packedMove) {
    uint64_t data = static_cast<uint32_t>(score);
    data |= (static_cast<uint64_t>(depth) & 0xFFULL) << 32;
    data |= (static_cast<uint64_t>(static_cast<int>(flag)) & 0x3ULL) << 40;
    data |= (static_cast<uint64_t>(packedMove) & 0xFFFFULL) << 42;
    return data;
}

void TranspositionTable::unpackData(uint64_t data, int& score, int& depth, TTFlag& flag, uint16_t& packedMove) {
    score = static_cast<int32_t>(static_cast<uint32_t>(data & 0xFFFFFFFFULL));
    depth = static_cast<int>((data >> 32) & 0xFFULL);
    flag = static_cast<TTFlag>((data >> 40) & 0x3ULL);
    packedMove = static_cast<uint16_t>((data >> 42) & 0xFFFFULL);
}

void TranspositionTable::resize(int mbSize) {
    delete[] table;
    table = nullptr;
    size = 0;

    if (mbSize <= 0) return;
    const size_t bytes = static_cast<size_t>(mbSize) * 1024ULL * 1024ULL;
    size_t count = bytes / sizeof(TTAtomicEntry);
    if (count == 0) count = 1;

    table = new TTAtomicEntry[count];
    size = count;
    clear();
}

void TranspositionTable::clear() {
    if (!table || size == 0) return;
    for (size_t i = 0; i < size; i++) {
        table[i].data.store(0, std::memory_order_relaxed);
        table[i].key.store(0, std::memory_order_relaxed);
    }
}

void TranspositionTable::store(uint64_t hash, int score, int depth, TTFlag flag, const Move& bestMove) {
    if (!table || size == 0) return;

    const size_t index = static_cast<size_t>(hash % size);
    TTAtomicEntry& e = table[index];

    const uint64_t existingKey = e.key.load(std::memory_order_relaxed);
    const uint64_t existingData = e.data.load(std::memory_order_relaxed);
    int existingScore = 0;
    int existingDepth = 0;
    TTFlag existingFlag = TTFlag::EXACT;
    uint16_t existingMove = 0;
    unpackData(existingData, existingScore, existingDepth, existingFlag, existingMove);

    // Depth-preferred replacement. If keys differ, we can overwrite.
    if (existingKey == 0 || existingKey != hash || depth >= existingDepth) {
        const uint16_t pm = packMove(bestMove);
        const uint64_t newData = packData(score, depth, flag, pm);
        e.data.store(newData, std::memory_order_relaxed);
        // Publish after data is written.
        e.key.store(hash, std::memory_order_release);
    }
}

bool TranspositionTable::probe(uint64_t key, int& outScore, int& outDepth, TTFlag& outFlag, Move& outMove) const {
    if (!table || size == 0) return false;

    const size_t index = static_cast<size_t>(key % size);
    const TTAtomicEntry& e = table[index];

    const uint64_t foundKey = e.key.load(std::memory_order_acquire);
    if (foundKey != key) return false;

    const uint64_t data = e.data.load(std::memory_order_relaxed);
    uint16_t packedMove = 0;
    unpackData(data, outScore, outDepth, outFlag, packedMove);
    outMove = unpackMove(packedMove);
    return true;
}

bool is_threefold_repetition(const std::vector<uint64_t>& history) {
    if (history.empty()) return false;
    uint64_t current = history.back();
    int count = 0;
    for (int i = static_cast<int>(history.size()) - 1; i >= 0; i--) {
        if (history[i] == current) {
            count++;
            if (count >= 3) return true;
        }
    }
    return false;
}

const int see_piece_values[] = {0, 100, 350, 350, 525, 900, 20000};

static int get_least_valuable_attacker(Board& board, int square, int bySide, int& outRow, int& outCol) { // bySide: Attacker side
    int r = square / 8;
    int c = square % 8;

    // pawn attacks
    int pawnDir = (bySide == 1) ? 1 : -1; 
    
    // check left and right diagonals - fix inverted pawn direction
    if (r - pawnDir >= 0 && r - pawnDir < 8) {
        if (c - 1 >= 0) {
            int p = board.squares[r - pawnDir][c - 1];
            if (p == (bySide * pawn)) {
                outRow = r - pawnDir;
                outCol = c - 1;
                return p; // Attacker pawn found
            }
        }
        if (c + 1 < 8) {
            int p = board.squares[r - pawnDir][c + 1];
            if (p == (bySide * pawn)) {
                outRow = r - pawnDir;
                outCol = c + 1;
                return p;
            }
        }
    }

    // knight attacks
    for (int i = 0; i < 8; ++i) {
        int nr = r + knight_moves[i][0];
        int nc = c + knight_moves[i][1];
        if (nr >= 0 && nr < 8 && nc >= 0 && nc < 8) {
            int p = board.squares[nr][nc];
            if (p == (bySide * knight)) {
                outRow = nr;
                outCol = nc;
                return p;
            }
        }
    }

    // Bishop and Queen (Diagonals)
    for (int i = 0; i < 4; ++i) {
        int nr = r + bishop_directions[i][0];
        int nc = c + bishop_directions[i][1];
        while (nr >= 0 && nr < 8 && nc >= 0 && nc < 8) {
            int p = board.squares[nr][nc];
            if (p != 0) {
                if (p == (bySide * bishop) || p == (bySide * queen)) {
                    outRow = nr;
                    outCol = nc;
                    return p;
                }
                break; // Blocked by another piece
            }
            nr += bishop_directions[i][0];
            nc += bishop_directions[i][1];
        }
    }

    // Rook and Queen (Straight lines)
    for (int i = 0; i < 4; ++i) {
        int nr = r + rook_directions[i][0];
        int nc = c + rook_directions[i][1];
        while (nr >= 0 && nr < 8 && nc >= 0 && nc < 8) {
            int p = board.squares[nr][nc];
            if (p != 0) {
                if (p == (bySide * rook) || p == (bySide * queen)) {
                    outRow = nr;
                    outCol = nc;
                    return p;
                }
                break; // Blocked by another piece
            }
            nr += rook_directions[i][0];
            nc += rook_directions[i][1];
        }
    }

    // King attacks
    for (int i = 0; i < 8; ++i) {
        int nr = r + king_moves[i][0];
        int nc = c + king_moves[i][1];
        if (nr >= 0 && nr < 8 && nc >= 0 && nc < 8) {
            int p = board.squares[nr][nc];
            if (p == (bySide * king)) {
                outRow = nr;
                outCol = nc;
                return p;
            }
        }
    }

    return 0; // no attacker found
}

int see_exchange(Board& board, Move& move) {
    // Initial gain: value of the piece on the target square
    int scores[32];
    int scoreIndex = 0;
    
    // Get the value of the captured piece without simulating the move
    int capturedVal = see_piece_values[std::abs(move.capturedPiece)];
    // If en-passant, use pawn value
    if (move.isEnPassant) capturedVal = see_piece_values[pawn];

    scores[scoreIndex++] = capturedVal;
    
    int attackerSide = board.isWhiteTurn ? 1 : -1;
    int currentAttacker = board.squares[move.fromRow][move.fromCol];
    
    int toSq = move.toRow * 8 + move.toCol;
    
    // Store all modified squares for restoration
    struct BoardChange {
        int row, col, piece;
    };
    BoardChange changes[32];
    int changeCount = 0;
    
    // Store original state
    changes[changeCount++] = {move.toRow, move.toCol, board.squares[move.toRow][move.toCol]};
    changes[changeCount++] = {move.fromRow, move.fromCol, board.squares[move.fromRow][move.fromCol]};
    
    // Make the initial move
    board.squares[move.toRow][move.toCol] = currentAttacker;
    board.squares[move.fromRow][move.fromCol] = 0;
    attackerSide = -attackerSide; // Turn passes to the opponent

    // Chain Reaction - simulate the full exchange sequence
    while (true) {
        int nextAttackerRow, nextAttackerCol;
        int nextAttacker = get_least_valuable_attacker(board, toSq, attackerSide, nextAttackerRow, nextAttackerCol);
        
        if (nextAttacker == 0) break; // No more attackers
        
        // Store the value of the piece being captured (currentAttacker)
        scores[scoreIndex++] = see_piece_values[std::abs(currentAttacker)];

        // Store the original state of the attacker's square before moving
        changes[changeCount++] = {nextAttackerRow, nextAttackerCol, board.squares[nextAttackerRow][nextAttackerCol]};
        
        // Update board state: move the attacker to the target square
        board.squares[move.toRow][move.toCol] = nextAttacker;
        board.squares[nextAttackerRow][nextAttackerCol] = 0;

        currentAttacker = nextAttacker;
        attackerSide = -attackerSide;
        
        if (scoreIndex > 30) break;  // to avoid endless loops
    }

    // Restore all board changes
    for (int i = changeCount - 1; i >= 0; i--) {
        board.squares[changes[i].row][changes[i].col] = changes[i].piece;
    }
    
    // Use negamax-style evaluation on the scores array
    // Work backwards from the last capture
    for (int i = scoreIndex - 1; i > 0; i--) {
        // Each side chooses the best outcome: capture and get the opponent's continuation,
        // or don't capture and keep 0
        scores[i - 1] = std::max(-scores[i] + scores[i - 1], 0);
    }
    
    return scores[0];
}