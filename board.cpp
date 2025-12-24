#include "board.h"
#include <iostream>
#include <algorithm>
#include <cmath>

// Piece encoding (constants declared in board.h)

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

// columns defined in main.cpp
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

inline int abs_int(int x) {
    return x < 0 ? -x : x;
}

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

    if (abs_int(piece) == 1 && move.fromCol != move.toCol && move.capturedPiece == 0) {
        move.isEnPassant = true;
        int captureRow = move.fromRow;
        squares[captureRow][move.toCol] = 0;
    }
    
    if (abs_int(piece) == 6 && abs_int(move.fromCol - move.toCol) == 2) {
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
    if (abs_int(piece) == 1) {
        int promotionRank = piece > 0 ? 0 : 7;
        if (move.toRow == promotionRank && move.promotion != 0) {
            squares[move.toRow][move.toCol] = (piece > 0) ? move.promotion : -move.promotion;
        }
    }

    if (abs_int(piece) == 1 && abs_int(move.fromRow - move.toRow) == 2) {
        enPassantCol = move.fromCol;
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

// Şimdi tüm move generation fonksiyonlarını buraya kopyala
std::vector<Move> generate_pawn_moves(const Board& board, int row, int col) {
    std::vector<Move> moves;
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
                moves.push_back(pm);
            }
        } else {
            moves.push_back(move);

            // Double move forward from starting position
            if ((color == 1 && row == 6) || (color == -1 && row == 1)) {
                int r2 = row + 2 * direction;
                if (board.squares[r2][col] == 0) {
                    Move dm = move;
                    dm.toRow = r2;
                    moves.push_back(dm);
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
                            moves.push_back(pm);
                        }
                    } else {
                        moves.push_back(m);
                    }
                }
            }
        }
    }

    // En passant (independent of forward square being empty)
    if (board.enPassantCol != -1 && abs_int(col - board.enPassantCol) == 1) {
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
                moves.push_back(m);
            }
        }
    }

    return moves;
}

std::vector<Move> generate_knight_moves(const Board& board, int row, int col) {
    std::vector<Move> moves;
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
                moves.push_back(m);
            }
        }
    }
    return moves;
}

std::vector<Move> generate_queen_moves(const Board& board, int row, int col) {
    std::vector<Move> diagonalmoves = generate_bishop_moves(board, row, col);
    std::vector<Move> straightmoves = generate_rook_moves(board, row, col);
    std::vector<Move> allmoves;
    allmoves.insert(allmoves.end(), diagonalmoves.begin(), diagonalmoves.end());
    allmoves.insert(allmoves.end(), straightmoves.begin(), straightmoves.end());
    return allmoves;
}

std::vector<Move> generate_king_moves(const Board& board, int row, int col) {
    std::vector<Move> moves;
    int piece = board.squares[row][col];
    int color = (piece > 0) ? 1 : -1;

    for (int i = 0; i < 8; ++i) {
        int r = row + king_moves[i][0];
        int c = col + king_moves[i][1];
        if (r >= 0 && r < 8 && c >= 0 && c < 8) {
            int target = board.squares[r][c];
            if (target == 0 || (target > 0 && color == -1) || (target < 0 && color == 1)) {
                Move m; m.fromRow = row; m.fromCol = col; m.toRow = r; m.toCol = c; m.capturedPiece = target; moves.push_back(m);
            }
        }
    }

    const bool opponentIsWhite = (color == -1);
    const bool kingInCheck = is_square_attacked(board, row, col, opponentIsWhite);

    if (!kingInCheck) {
        // Black castles
        if (color == -1 && row == 0 && col == 4) {
            if (board.blackCanCastleKingSide) {
                if (board.squares[0][5] == 0 && board.squares[0][6] == 0) {
                    if (!is_square_attacked(board, 0, 5, true) && !is_square_attacked(board, 0, 6, true)) {
                        Move m; m.fromRow = row; m.fromCol = col; m.toRow = 0; m.toCol = 6; m.isCastling = true; moves.push_back(m);
                    }
                }
            }
            if (board.blackCanCastleQueenSide) {
                if (board.squares[0][1] == 0 && board.squares[0][2] == 0 && board.squares[0][3] == 0) {
                    if (!is_square_attacked(board, 0, 3, true) && !is_square_attacked(board, 0, 2, true)) {
                        Move m; m.fromRow = row; m.fromCol = col; m.toRow = 0; m.toCol = 2; m.isCastling = true; moves.push_back(m);
                    }
                }
            }
        }

        // White castles
        if (color == 1 && row == 7 && col == 4) {
            if (board.whiteCanCastleKingSide) {
                if (board.squares[7][5] == 0 && board.squares[7][6] == 0) {
                    if (!is_square_attacked(board, 7, 5, false) && !is_square_attacked(board, 7, 6, false)) {
                        Move m; m.fromRow = row; m.fromCol = col; m.toRow = 7; m.toCol = 6; m.isCastling = true; moves.push_back(m);
                    }
                }
            }
            if (board.whiteCanCastleQueenSide) {
                if (board.squares[7][1] == 0 && board.squares[7][2] == 0 && board.squares[7][3] == 0) {
                    if (!is_square_attacked(board, 7, 3, false) && !is_square_attacked(board, 7, 2, false)) {
                        Move m; m.fromRow = row; m.fromCol = col; m.toRow = 7; m.toCol = 2; m.isCastling = true; moves.push_back(m);
                    }
                }
            }
        }
    }
    return moves;
}

// ... diğer generate fonksiyonları

std::vector<Move> generate_bishop_moves(const Board& board, int row, int col) {
    int piece = board.squares[row][col];
    int color = (piece > 0) ? 1 : -1; // 1 for white, -1 for black
    int moveCount = 0;
    std::vector<Move> moves;
    for (int d = 0; d < 4; ++d) {
        int r = row + bishop_directions[d][0];
        int c = col + bishop_directions[d][1];
        while (r >= 0 && r < 8 && c >= 0 && c < 8) {
            int target = board.squares[r][c];
            if (target == 0) {
                Move m; m.fromRow = row; m.fromCol = col; m.toRow = r; m.toCol = c; m.capturedPiece = 0; moves.push_back(m);
            } else {
                if ((target > 0 && color == -1) || (target < 0 && color == 1)) {
                    Move m; m.fromRow = row; m.fromCol = col; m.toRow = r; m.toCol = c; m.capturedPiece = target; moves.push_back(m);
                }
                break;
            }
            r += bishop_directions[d][0];
            c += bishop_directions[d][1];
        }
    }
    return moves;
}

std::vector<Move> generate_rook_moves(const Board& board, int row, int col) {
    int piece = board.squares[row][col];
    int color = (piece > 0) ? 1 : -1; // 1 for white, -1 for black
    int moveCount = 0;
    std::vector<Move> moves;

    for (int d = 0; d < 4; ++d) {
        int r = row + rook_directions[d][0];
        int c = col + rook_directions[d][1];
        while (r >= 0 && r < 8 && c >= 0 && c < 8) {
            int target = board.squares[r][c];
            if (target == 0) {
                Move m; m.fromRow = row; m.fromCol = col; m.toRow = r; m.toCol = c; m.capturedPiece = 0; moves.push_back(m);
            } else {
                if ((target > 0 && color == -1) || (target < 0 && color == 1)) {
                    Move m; m.fromRow = row; m.fromCol = col; m.toRow = r; m.toCol = c; m.capturedPiece = target; moves.push_back(m);
                }
                break;
            }
            r += rook_directions[d][0];
            c += rook_directions[d][1];
        }
    }
    return moves;
}

std::vector<Move> get_all_moves(const Board& board, bool isWhiteTurn) {
    std::vector<Move> allMoves;
    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 8; ++c) {
            int piece = board.squares[r][c];
            if (piece == 0) continue;
            if (isWhiteTurn && piece < 0) continue;
            if (!isWhiteTurn && piece > 0) continue;
            switch (abs_int(piece)) {
                case pawn:
                    {
                        auto v = generate_pawn_moves(board, r, c);
                        allMoves.insert(allMoves.end(), v.begin(), v.end());
                    }
                    break;
                case knight:
                    {
                        auto v = generate_knight_moves(board, r, c);
                        allMoves.insert(allMoves.end(), v.begin(), v.end());
                    }
                    break;
                case bishop:
                    {
                        auto v = generate_bishop_moves(board, r, c);
                        allMoves.insert(allMoves.end(), v.begin(), v.end());
                    }
                    break;
                case rook:
                    {
                        auto v = generate_rook_moves(board, r, c);
                        allMoves.insert(allMoves.end(), v.begin(), v.end());
                    }
                    break;
                case queen:
                    {
                        auto v = generate_queen_moves(board, r, c);
                        allMoves.insert(allMoves.end(), v.begin(), v.end());
                    }
                    break;
                case king:
                    {
                        auto v = generate_king_moves(board, r, c);
                        allMoves.insert(allMoves.end(), v.begin(), v.end());
                    }
                    break;
            }
        }
    }
    return allMoves;
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
            int absP = abs_int(piece);
            std::vector<Move> gen;
            switch (absP) {
                case pawn: gen = generate_pawn_moves(board, row, col); break;
                case knight: gen = generate_knight_moves(board, row, col); break;
                case bishop: gen = generate_bishop_moves(board, row, col); break;
                case rook: gen = generate_rook_moves(board, row, col); break;
                case queen: gen = generate_queen_moves(board, row, col); break;
                case king: gen = generate_king_moves(board, row, col); break;
            }
            for (auto &m : gen) {
                if (m.capturedPiece != 0 || m.isEnPassant) moves.push_back(m);
            }
        }
    }
    return moves;
}

bool is_square_attacked(const Board& board, int row, int col, bool isWhiteAttacker) {
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
                if ((p > 0) == isWhiteAttacker && (abs_int(p) == queen || abs_int(p) == rook)) return true;
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
                if ((p > 0) == isWhiteAttacker && (abs_int(p) == queen || abs_int(p) == bishop)) return true;
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
                int ap = abs_int(p);
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
    int absP = abs_int(piece);
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

// Transposition Table Definition
std::unordered_map<uint64_t, TTEntry> transpositionTable;

void storeInTT(uint64_t key, int score, int depth, TTFlag flag, Move bestMove, 
               std::unordered_map<uint64_t, TTEntry>& table) {
    TTEntry entry;
    entry.hash = key;
    entry.score = score;
    entry.depth = depth;
    entry.flag = flag;
    entry.bestMove = bestMove;
    table[key] = entry;  // Insert or update the entry
}

TTEntry* probeTranspositionTable(uint64_t key, std::unordered_map<uint64_t, TTEntry>& table) {
    auto it = table.find(key);
    if (it != table.end() && it->second.hash == key) {
        return &it->second;  // Found, return pointer
    }
    return nullptr;  // Not found
}

Move isInTranspositionTable(uint64_t key, const std::unordered_map<uint64_t, TTEntry>& table) {
    auto it = table.find(key);
    if (it != table.end()) {
        return it->second.bestMove;
    }
    return Move();
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