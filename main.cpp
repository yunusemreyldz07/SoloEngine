#include <iostream>
#include <vector>
#include <cctype>
#include <string>
#include <algorithm>
#include <sstream>
#include <cstdint>

class Board; // forward declaration

using namespace std;

// Piece encoding
const int empty_sqr = 0;
const int pawn = 1;
const int knight = 2;
const int bishop = 3;
const int rook = 4;
const int queen = 5;
const int king = 6;

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

struct Move {
    int fromRow, fromCol;
    int toRow, toCol;
    int capturedPiece; 
    int promotion;

    bool prevW_KingSide;
    bool prevW_QueenSide;
    bool prevB_KingSide;
    bool prevB_QueenSide;

    int prevEnPassantCol;

    bool isEnPassant;
    bool isCastling;

    Move()
        : fromRow(0), fromCol(0), toRow(0), toCol(0), capturedPiece(0), promotion(0),
          prevW_KingSide(false), prevW_QueenSide(false), prevB_KingSide(false), prevB_QueenSide(false),
          prevEnPassantCol(-1), isEnPassant(false), isCastling(false) {}
};

const int PIECE_VALUES[] = {0, 100, 320, 330, 500, 900, 20000};

char columns[]  = "abcdefgh";

inline int abs_int(int x) {
    return x < 0 ? -x : x;
}

inline char to_upper_ascii(char c) {
    return (c >= 'a' && c <= 'z') ? static_cast<char>(c - 'a' + 'A') : c;
}

// Declaration only; definition is after Board is fully defined.
void printBoard(const Board& board);

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

class Board {
public:
    int squares[8][8];
    bool isWhiteTurn;
    
    bool whiteCanCastleKingSide;
    bool whiteCanCastleQueenSide;
    bool blackCanCastleKingSide;
    bool blackCanCastleQueenSide;
    
    // -1 if no en passant available
    int enPassantCol; 

    Board() {
        resetBoard();
    }

    void resetBoard() {
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
    }
    
    void makeMove(Move& move) {
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
                squares[move.fromRow][5] = squares[move.fromRow][7];
                squares[move.fromRow][7] = 0;
            } 
            else { 
                squares[move.fromRow][3] = squares[move.fromRow][0];
                squares[move.fromRow][0] = 0;
            }
            move.isCastling = true;
        }

        // Promotion: piyon son sıraya geldiyse taşı yükselt
        if (abs_int(piece) == 1) {
            int promotionRank = piece > 0 ? 0 : 7;
            if (move.toRow == promotionRank) {
                int promoType = move.promotion != 0 ? abs_int(move.promotion) : queen; // store type (q/r/b/n)
                int promoPiece = (piece > 0) ? promoType : -promoType;
                move.promotion = promoPiece;
                squares[move.toRow][move.toCol] = promoPiece;
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

        else if (piece == -4 && move.fromRow == 0 && move.fromCol == 0){
            blackCanCastleQueenSide = false;
        }

        else if (piece == -4 && move.fromRow == 0 && move.fromCol == 7){
            blackCanCastleKingSide = false;
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
    }
    
    void unmakeMove(Move& move) {
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
                squares[move.fromRow][7] = squares[move.fromRow][5];
                squares[move.fromRow][5] = 0;
            } else {
                squares[move.fromRow][0] = squares[move.fromRow][3];
                squares[move.fromRow][3] = 0;
            }
        }

        
        whiteCanCastleKingSide = move.prevW_KingSide;
        whiteCanCastleQueenSide = move.prevW_QueenSide;
        blackCanCastleKingSide = move.prevB_KingSide;
        blackCanCastleQueenSide = move.prevB_QueenSide;
        enPassantCol = move.prevEnPassantCol;
    }
};

// Tahtayı harflerle (P, n, k...) yazdıran fonksiyon
void printBoard(const Board& board) {
    cout << "  +-----------------+" << endl;
    for (int r = 0; r < 8; r++) {
        cout << 8 - r << " | "; // Satır numarası
        for (int c = 0; c < 8; c++) {
            int p = board.squares[r][c];
            char cStr = '.';
            // Sayıları harfe çevir
            if (p != 0) {
                char symbols[] = "?pnbrqk"; // 1=p, 2=n...
                cStr = symbols[abs_int(p)];
                if (p > 0) cStr = to_upper_ascii(cStr); // Beyazsa BÜYÜK harf
            }
            cout << cStr << " ";
        }
        cout << "|" << endl;
    }
    cout << "  +-----------------+" << endl;
    cout << "    a b c d e f g h" << endl;
}

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

int pawn_moves[4][2] = {
    {1, 0}, // White pawn moves up
    {-1, 0}, // Black pawn moves down
    {2, 0}, // White pawn double move
    {-2, 0} // Black pawn double move
};

int pawn_takes[4][2] = {
    {1, -1}, {1, 1},
    {-1, -1}, {-1, 1} 
};

// PST arrays would be defined here

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

static bool is_endgame(const Board& board) {
    int whiteQueens = 0;
    int blackQueens = 0;
    int whiteRooks = 0;
    int blackRooks = 0;
    int whiteOther = 0; // non-pawn, non-king, non-queen pieces
    int blackOther = 0;
    int whiteMinors = 0; // knights + bishops (only for the “one minor max” rule)
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
            if (absP == king || absP == pawn) {
                continue; // ignored for phase detection
            }
            if (absP == rook) {
                if (isWhite) blackRooks += 0; // no-op, keep structure simple
                if (isWhite) whiteRooks++; else blackRooks++;
            }
            if (absP == knight || absP == bishop) {
                if (isWhite) whiteMinors++; else blackMinors++;
            }

            if (isWhite) whiteOther++; else blackOther++;
        }
    }

    // 1) No queens at all => endgame.
    if (whiteQueens == 0 && blackQueens == 0) return true;

    // 2) If a side has a queen, it must have no rooks and at most one minor piece.
    // Pawns are ignored.
    if (whiteQueens > 0) {
        if (whiteRooks > 0) return false;
        if (whiteOther > 1) return false;
        if (whiteOther == 1 && whiteMinors != 1) return false; // if only one piece, it must be a minor
    }
    if (blackQueens > 0) {
        if (blackRooks > 0) return false;
        if (blackOther > 1) return false;
        if (blackOther == 1 && blackMinors != 1) return false;
    }

    return true;
}

// By the way, a reminder for myself; remember to flip the PST for black pieces when evaluating.
// Also, here is how we understand it is the endgame 
// 1. Both sides have no queens or
// 2. Every side which has a queen has additionally no other pieces or one minorpiece maximum. Pawns are ignored... I guess.

bool is_square_attacked(const Board& board, int row, int col, bool isWhiteAttacker);

vector<Move> get_all_moves(const Board& board, bool isWhiteTurn = true);

int evaulate_board(const Board& board);

struct Zobrist {
    uint64_t piece[12][64]{};
    uint64_t castling[16]{};
    uint64_t epFile[9]{};
    uint64_t side{};

    static uint64_t splitmix64(uint64_t& x) {
        uint64_t z = (x += 0x9E3779B97F4A7C15ULL);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
        return z ^ (z >> 31);
    }

    Zobrist() {
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
};

static const Zobrist& zobrist() {
    static Zobrist z;
    return z;
}

static int piece_to_zobrist_index(int piece) {
    int absP = abs_int(piece);
    if (absP < 1 || absP > 6) return -1;
    int base = (piece > 0) ? 0 : 6;
    return base + (absP - 1);
}

static uint64_t position_key(const Board& board) {
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

static bool is_threefold_repetition(const vector<uint64_t>& history) {
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

static int repetition_draw_score(const Board& board) {
    // Small contempt: avoid drawing by repetition when ahead.
    int standPat = evaulate_board(board);
    const int contempt = 100;
    if (standPat > 200) return -contempt;
    if (standPat < -200) return contempt;
    return 0;
}

vector<Move> generate_rook_moves(const Board& board, int row, int col) {
    int piece = board.squares[row][col];
    int color = (piece > 0) ? 1 : -1; // 1 for white, -1 for black
    int moveCount = 0;
    vector<Move> moves; // Max 14 moves for rook, extra buffer

    for (int d = 0; d < 4; ++d) {
        int r = row + rook_directions[d][0];
        int c = col + rook_directions[d][1];
        while (r >= 0 && r < 8 && c >= 0 && c < 8) {
            int target = board.squares[r][c];
            if (target == 0) {
                Move move;
                move.fromRow = row;
                move.fromCol = col;
                move.toRow = r;
                move.toCol = c;
                moveCount++;
                moves.push_back(move);
            } else {
                if ((target > 0 && color == -1) || (target < 0 && color == 1)) {
                    Move move;
                    move.fromRow = row;
                    move.fromCol = col;
                    move.toRow = r;
                    move.toCol = c;
                    move.capturedPiece = target;
                    moveCount++;
                    moves.push_back(move);
                }
                break;
            }
            r += rook_directions[d][0];
            c += rook_directions[d][1];
        }
    }
    return moves;
}

vector<Move> generate_bishop_moves(const Board& board, int row, int col) {
    int piece = board.squares[row][col];
    int color = (piece > 0) ? 1 : -1; // 1 for white, -1 for black
    int moveCount = 0;
    vector<Move> moves;
    for (int d = 0; d < 4; ++d) {
        int r = row + bishop_directions[d][0];
        int c = col + bishop_directions[d][1];
        while (r >= 0 && r < 8 && c >= 0 && c < 8) {
            int target = board.squares[r][c];
            if (target == 0) {
                Move move;
                move.fromRow = row;
                move.fromCol = col;
                move.toRow = r;
                move.toCol = c;
                moveCount++;
                moves.push_back(move);
            } else {
                if ((target > 0 && color == -1) || (target < 0 && color == 1)) {
                    Move move;
                    move.fromRow = row;
                    move.fromCol = col;
                    move.toRow = r;
                    move.toCol = c;
                    move.capturedPiece = target;
                    moveCount++;
                    moves.push_back(move);
                }
                break;
            }
            r += bishop_directions[d][0];
            c += bishop_directions[d][1];
        }
    }
    return moves;
}

vector<Move> generate_queen_moves(const Board& board, int row, int col) {
    vector<Move> diagonalmoves = generate_bishop_moves(board, row, col);
    vector<Move> straightmoves = generate_rook_moves(board, row, col);
    // Combine all moves
    // allmoves = diagonalmoves + straightmoves
    
    vector<Move> allmoves;
    allmoves.insert(allmoves.end(), diagonalmoves.begin(), diagonalmoves.end());
    allmoves.insert(allmoves.end(), straightmoves.begin(), straightmoves.end());
    return allmoves;
}

vector<Move> generate_knight_moves(const Board& board, int row, int col) {
    vector<Move> moves;
    int piece = board.squares[row][col];
    int color = (piece > 0) ? 1 : -1; // 1 for white, -1 for black

    for (int i = 0; i < 8; ++i) {
        int r = row + knight_moves[i][0];
        int c = col + knight_moves[i][1];
        if (r >= 0 && r < 8 && c >= 0 && c < 8) {
            int target = board.squares[r][c];
            if (target == 0 || (target > 0 && color == -1) || (target < 0 && color == 1)) {
                Move move;
                move.fromRow = row;
                move.fromCol = col;
                move.toRow = r;
                move.toCol = c;
                if (target != 0) {
                    move.capturedPiece = target;
                }
                moves.push_back(move);
            }
        }
    }
    return moves;
}

vector<Move> generate_king_moves(const Board& board, int row, int col) {
    vector<Move> moves;
    int piece = board.squares[row][col];
    int color = (piece > 0) ? 1 : -1; // 1 for white, -1 for black

    for (int i = 0; i < 8; ++i) {
        int r = row + king_moves[i][0];
        int c = col + king_moves[i][1];
        if (r >= 0 && r < 8 && c >= 0 && c < 8) {
            int target = board.squares[r][c];
            if (target == 0 || (target > 0 && color == -1) || (target < 0 && color == 1)) {
                Move move;
                move.fromRow = row;
                move.fromCol = col;
                move.toRow = r;
                move.toCol = c;
                if (target != 0) {
                    move.capturedPiece = target;
                }
                moves.push_back(move);
            }
        }
    }

    const bool opponentIsWhite = (color == -1);
    const bool kingInCheck = is_square_attacked(board, row, col, opponentIsWhite);

    if (!kingInCheck) {
        // Black castles
        if (color == -1 && row == 0 && col == 4) {
            if (board.blackCanCastleKingSide) {
                if (board.squares[0][7] == -rook && board.squares[0][5] == 0 && board.squares[0][6] == 0) {
                    if (!is_square_attacked(board, 0, 5, true) && !is_square_attacked(board, 0, 6, true)) {
                        Move move;
                        move.fromRow = row;
                        move.fromCol = col;
                        move.toRow = 0;
                        move.toCol = 6;
                        move.isCastling = true;
                        moves.push_back(move);
                    }
                }
            }
            if (board.blackCanCastleQueenSide) {
                if (board.squares[0][0] == -rook && board.squares[0][1] == 0 && board.squares[0][2] == 0 && board.squares[0][3] == 0) {
                    if (!is_square_attacked(board, 0, 3, true) && !is_square_attacked(board, 0, 2, true)) {
                        Move move;
                        move.fromRow = row;
                        move.fromCol = col;
                        move.toRow = 0;
                        move.toCol = 2;
                        move.isCastling = true;
                        moves.push_back(move);
                    }
                }
            }
        }

        // White castles
        if (color == 1 && row == 7 && col == 4) {
            if (board.whiteCanCastleKingSide) {
                if (board.squares[7][7] == rook && board.squares[7][5] == 0 && board.squares[7][6] == 0) {
                    if (!is_square_attacked(board, 7, 5, false) && !is_square_attacked(board, 7, 6, false)) {
                        Move move;
                        move.fromRow = row;
                        move.fromCol = col;
                        move.toRow = 7;
                        move.toCol = 6;
                        move.isCastling = true;
                        moves.push_back(move);
                    }
                }
            }
            if (board.whiteCanCastleQueenSide) {
                if (board.squares[7][0] == rook && board.squares[7][1] == 0 && board.squares[7][2] == 0 && board.squares[7][3] == 0) {
                    if (!is_square_attacked(board, 7, 3, false) && !is_square_attacked(board, 7, 2, false)) {
                        Move move;
                        move.fromRow = row;
                        move.fromCol = col;
                        move.toRow = 7;
                        move.toCol = 2;
                        move.isCastling = true;
                        moves.push_back(move);
                    }
                }
            }
        }
    }
    return moves;
}

vector<Move> generate_pawn_moves(const Board& board, int row, int col) {
    vector<Move> moves;
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
                Move promoMove = move;
                promoMove.promotion = promoType; // store absolute type
                moves.push_back(promoMove);
            }
        } else {
            moves.push_back(move);

            // Double move forward from starting position
            if ((color == 1 && row == 6) || (color == -1 && row == 1)) {
                int r2 = row + 2 * direction;
                if (r2 >= 0 && r2 < 8 && board.squares[r2][col] == 0) {
                    Move move2;
                    move2.fromRow = row;
                    move2.fromCol = col;
                    move2.toRow = r2;
                    move2.toCol = col;
                    moves.push_back(move2);
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
                if ((target > 0 && color == -1) || (target < 0 && color == 1)) {
                    Move move;
                    move.fromRow = row;
                    move.fromCol = col;
                    move.toRow = r;
                    move.toCol = c;
                    move.capturedPiece = target;

                    // Promotion on capture
                    if (r == promotionRank) {
                        for (int promoType : {queen, rook, bishop, knight}) {
                            Move promoMove = move;
                            promoMove.promotion = promoType; // store absolute type
                            moves.push_back(promoMove);
                        }
                    } else {
                        moves.push_back(move);
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
                // Only valid from the correct rank (white: row 3, black: row 4)
                if ((color == 1 && row == 3) || (color == -1 && row == 4)) {
                    Move epMove;
                    epMove.fromRow = row;
                    epMove.fromCol = col;
                    epMove.toRow = epTargetRow;
                    epMove.toCol = epPawnCol;
                    epMove.isEnPassant = true;
                    moves.push_back(epMove);
                }
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
            // Check if it's an attacking pawn by its color
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
                if (p == (isWhiteAttacker ? 4 : -4) || p == (isWhiteAttacker ? 5 : -5)) return true;
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
                if (p == (isWhiteAttacker ? 3 : -3) || p == (isWhiteAttacker ? 5 : -5)) return true;
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

    return false; // No attackers found
}

vector<Move> get_all_moves(const Board& board, bool isWhiteTurn) {
    vector<Move> allMoves;
    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 8; ++c) {
            int piece = board.squares[r][c];
            if (piece == 0) continue;
            if (isWhiteTurn && piece < 0) continue;
            if (!isWhiteTurn && piece > 0) continue;
            vector<Move> pieceMoves;
            switch (abs_int(piece)) {
                case pawn:
                    pieceMoves = generate_pawn_moves(board, r, c);
                    break;
                case knight:
                    pieceMoves = generate_knight_moves(board, r, c);
                    break;
                case bishop:
                    pieceMoves = generate_bishop_moves(board, r, c);
                    break;
                case rook:
                    pieceMoves = generate_rook_moves(board, r, c);
                    break;
                case queen:
                    pieceMoves = generate_queen_moves(board, r, c);
                    break;
                case king:
                    pieceMoves = generate_king_moves(board, r, c);
                    break;
            }
            allMoves.insert(allMoves.end(), pieceMoves.begin(), pieceMoves.end());
        }
    }
    return allMoves;
}


int evaulate_board(const Board& board) {
    int score = 0;
    const bool endgame = is_endgame(board);
    
    for (int r=0; r<8; r++) {
        for (int c=0; c<8; c++) {
            int piece = board.squares[r][c];
            if (piece == 0) continue;
            int absPiece = abs_int(piece);
            int pstRow = (piece > 0) ? r : (7 - r); // PSTs are defined from White's perspective
            int pstValue = 0;
            switch (absPiece) {
                case pawn:
                    pstValue = pawn_pst[pstRow][c];
                    break;
                case knight:
                    pstValue = knight_pst[pstRow][c];
                    break;
                case bishop:
                    pstValue = bishop_pst[pstRow][c];
                    break;
                case rook:
                    pstValue = rook_pst[pstRow][c];
                    break;
                case queen:
                    pstValue = queen_pst[pstRow][c];
                    break;
                case king:
                    pstValue = endgame ? eg_king_pst[pstRow][c] : mg_king_pst[pstRow][c];
                    break;
            }
            int pieceValue = 0;
            switch (absPiece) {
                case pawn:
                    pieceValue = 100;
                    break;
                case knight:
                    pieceValue = 320;
                    break;
                case bishop:
                    pieceValue = 330;
                    break;
                case rook:
                    pieceValue = 500;
                    break;
                case queen:
                    pieceValue = 900;
                    break;
                case king:
                    pieceValue = 20000;
                    break;
            }
            int extra = 0;
            if (absPiece == pawn) {
                // Encourage advancing pawns; promote plans matter a lot at low depth.
                int advance = (piece > 0) ? (6 - r) : (r - 1);
                if (advance > 0) extra += advance * 6;

                // Simple passed pawn bonus: no enemy pawn on same file ahead.
                bool passed = true;
                if (piece > 0) {
                    for (int rr = r - 1; rr >= 0; rr--) {
                        if (board.squares[rr][c] == -pawn) { passed = false; break; }
                    }
                } else {
                    for (int rr = r + 1; rr < 8; rr++) {
                        if (board.squares[rr][c] == pawn) { passed = false; break; }
                    }
                }
                if (passed && advance > 0) extra += advance * 10;
            }

            if (piece > 0) {
                score += pieceValue + pstValue + extra;
            } else {
                score -= pieceValue + pstValue + extra;
            }
        }
    }
    return score;
}

// Ordering moves for better alpha-beta pruning
int scoreMove(const Board& board, const Move& move) {

    if (move.capturedPiece != 0 || move.isEnPassant) {
        int victimPiece = move.isEnPassant ? pawn : abs_int(move.capturedPiece);
        int victimValue = PIECE_VALUES[victimPiece];
        
        int attackerPiece = board.squares[move.fromRow][move.fromCol];
        int attackerValue = PIECE_VALUES[abs_int(attackerPiece)];

        return 10000 + (victimValue * 10) - attackerValue;
    }

    if (move.promotion != 0) {
        return 9000;
    }

    if (move.isCastling) {
        return 5000;
    }


    return 0;
}

vector<Move> get_capture_moves(const Board& board) {
    vector<Move> moves;
    bool whiteToMove = board.isWhiteTurn;

    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            int piece = board.squares[row][col];
            if (piece == 0) continue;
            if (whiteToMove && piece < 0) continue;
            if (!whiteToMove && piece > 0) continue;

            int color = (piece > 0) ? 1 : -1;
            int absPiece = abs_int(piece);

            if (absPiece == pawn) {
                int direction = (color == 1) ? -1 : 1;
                int promotionRank = (color == 1) ? 0 : 7;
                int r = row + direction;

                // Promotion quiet moves (rare but important for quiescence)
                if (r >= 0 && r < 8 && r == promotionRank && board.squares[r][col] == 0) {
                    for (int promoType : {queen, rook, bishop, knight}) {
                        Move promoMove;
                        promoMove.fromRow = row;
                        promoMove.fromCol = col;
                        promoMove.toRow = r;
                        promoMove.toCol = col;
                        promoMove.promotion = promoType;
                        moves.push_back(promoMove);
                    }
                }

                // Diagonal captures (+ promotion captures)
                if (r >= 0 && r < 8) {
                    for (int dc = -1; dc <= 1; dc += 2) {
                        int c = col + dc;
                        if (c < 0 || c >= 8) continue;
                        int target = board.squares[r][c];
                        if ((target > 0 && color == -1) || (target < 0 && color == 1)) {
                            if (r == promotionRank) {
                                for (int promoType : {queen, rook, bishop, knight}) {
                                    Move capPromo;
                                    capPromo.fromRow = row;
                                    capPromo.fromCol = col;
                                    capPromo.toRow = r;
                                    capPromo.toCol = c;
                                    capPromo.capturedPiece = target;
                                    capPromo.promotion = promoType;
                                    moves.push_back(capPromo);
                                }
                            } else {
                                Move cap;
                                cap.fromRow = row;
                                cap.fromCol = col;
                                cap.toRow = r;
                                cap.toCol = c;
                                cap.capturedPiece = target;
                                moves.push_back(cap);
                            }
                        }
                    }
                }

                // En passant
                if (board.enPassantCol != -1 && abs_int(col - board.enPassantCol) == 1) {
                    int epTargetRow = row + direction;
                    if (epTargetRow >= 0 && epTargetRow < 8) {
                        int epPawnRow = row;
                        int epPawnCol = board.enPassantCol;
                        int expectedEnemyPawn = (color == 1) ? -pawn : pawn;
                        if (board.squares[epTargetRow][epPawnCol] == 0 && board.squares[epPawnRow][epPawnCol] == expectedEnemyPawn) {
                            if ((color == 1 && row == 3) || (color == -1 && row == 4)) {
                                Move epMove;
                                epMove.fromRow = row;
                                epMove.fromCol = col;
                                epMove.toRow = epTargetRow;
                                epMove.toCol = epPawnCol;
                                epMove.isEnPassant = true;
                                moves.push_back(epMove);
                            }
                        }
                    }
                }
            }
            else if (absPiece == knight) {
                for (int i = 0; i < 8; i++) {
                    int r = row + knight_moves[i][0];
                    int c = col + knight_moves[i][1];
                    if (r < 0 || r >= 8 || c < 0 || c >= 8) continue;
                    int target = board.squares[r][c];
                    if ((target > 0 && color == -1) || (target < 0 && color == 1)) {
                        Move cap;
                        cap.fromRow = row;
                        cap.fromCol = col;
                        cap.toRow = r;
                        cap.toCol = c;
                        cap.capturedPiece = target;
                        moves.push_back(cap);
                    }
                }
            }
            else if (absPiece == bishop || absPiece == rook || absPiece == queen) {
                const int (*dirs)[2] = nullptr;
                int dirCount = 0;
                if (absPiece == bishop) { dirs = bishop_directions; dirCount = 4; }
                else if (absPiece == rook) { dirs = rook_directions; dirCount = 4; }
                else { dirs = queen_directions; dirCount = 8; }

                for (int d = 0; d < dirCount; d++) {
                    for (int dist = 1; dist < 8; dist++) {
                        int r = row + dirs[d][0] * dist;
                        int c = col + dirs[d][1] * dist;
                        if (r < 0 || r >= 8 || c < 0 || c >= 8) break;
                        int target = board.squares[r][c];
                        if (target == 0) continue;
                        if ((target > 0 && color == -1) || (target < 0 && color == 1)) {
                            Move cap;
                            cap.fromRow = row;
                            cap.fromCol = col;
                            cap.toRow = r;
                            cap.toCol = c;
                            cap.capturedPiece = target;
                            moves.push_back(cap);
                        }
                        break; // stop ray on first piece
                    }
                }
            }
            else if (absPiece == king) {
                for (int i = 0; i < 8; i++) {
                    int r = row + king_moves[i][0];
                    int c = col + king_moves[i][1];
                    if (r < 0 || r >= 8 || c < 0 || c >= 8) continue;
                    int target = board.squares[r][c];
                    if ((target > 0 && color == -1) || (target < 0 && color == 1)) {
                        Move cap;
                        cap.fromRow = row;
                        cap.fromCol = col;
                        cap.toRow = r;
                        cap.toCol = c;
                        cap.capturedPiece = target;
                        moves.push_back(cap);
                    }
                }
            }
        }
    }

    return moves;
}

int quiescence(Board& board, int alpha, int beta, int ply, vector<uint64_t>& history) {
    if (is_threefold_repetition(history)) {
        return repetition_draw_score(board);
    }

    bool whiteToMove = board.isWhiteTurn;
    int standPat = evaulate_board(board);

    if (whiteToMove) {
        if (standPat >= beta) return beta;
        if (standPat > alpha) alpha = standPat;
    } else {
        if (standPat <= alpha) return alpha;
        if (standPat < beta) beta = standPat;
    }

    vector<Move> moves = get_capture_moves(board);
    std::sort(moves.begin(), moves.end(), [&](const Move& a, const Move& b) {
        return scoreMove(board, a) > scoreMove(board, b);
    });

    for (Move& move : moves) {

        board.makeMove(move);
        history.push_back(position_key(board));

        // Reject illegal moves that leave our own king in check.
        bool sideJustMovedWasWhite = !board.isWhiteTurn;
        int kingPiece = sideJustMovedWasWhite ? 6 : -6;
        int kingRow = -1, kingCol = -1;
        for (int r = 0; r < 8; r++) {
            for (int c = 0; c < 8; c++) {
                if (board.squares[r][c] == kingPiece) { kingRow = r; kingCol = c; break; }
            }
            if (kingRow != -1) break;
        }
        if (kingRow == -1 || is_square_attacked(board, kingRow, kingCol, board.isWhiteTurn)) {
            history.pop_back();
            board.unmakeMove(move);
            continue;
        }

        int score = quiescence(board, alpha, beta, ply + 1, history);
        history.pop_back();
        board.unmakeMove(move);

        if (whiteToMove) {
            if (score > alpha) alpha = score;
        } else {
            if (score < beta) beta = score;
        }

        if (alpha >= beta) break;
    }

    return whiteToMove ? alpha : beta;
}

int minimax(Board& board, int depth, int alpha, int beta, int ply, vector<uint64_t>& history) {
    if (is_threefold_repetition(history)) {
        return repetition_draw_score(board);
    }

    if (depth == 0) {
        return quiescence(board, alpha, beta, ply, history);
    }

    bool whiteToMove = board.isWhiteTurn;
    vector<Move> possibleMoves = get_all_moves(board, whiteToMove);
    
    std::sort(possibleMoves.begin(), possibleMoves.end(), [&](const Move& a, const Move& b) {
        return scoreMove(board, a) > scoreMove(board, b);
    });

    constexpr int MATE_SCORE = 100000;
    int legalMoveCount = 0;
    
    if (whiteToMove) {
        int maxEval = -1000000000;
        for (Move& move : possibleMoves) {
            
            board.makeMove(move);
            history.push_back(position_key(board));

            int kingPiece = 6;
            int kingRow = -1, kingCol = -1;
            
            // Find king position
            for(int r=0; r<8; r++) {
                for(int c=0; c<8; c++) {
                    if(board.squares[r][c] == kingPiece) { kingRow=r; kingCol=c; break; }
                }
                if(kingRow != -1) break;
            }
            
            if (is_square_attacked(board, kingRow, kingCol, false)) {
                history.pop_back();
                board.unmakeMove(move); 
                continue;
            }
            
            legalMoveCount++;
            
            int eval = minimax(board, depth - 1, alpha, beta, ply + 1, history);
            history.pop_back();
            board.unmakeMove(move);
            
            maxEval = std::max(maxEval, eval);
            alpha = std::max(alpha, eval);
            if (beta <= alpha) break; // Beta Cut-off
        }
        
        // No legal moves means Checkmate or Stalemate
        if (legalMoveCount == 0) {
            // Is our king in check
             int kingPiece = 6;
             int kingRow = -1, kingCol = -1;
             for(int r=0; r<8; r++) { for(int c=0; c<8; c++) { if(board.squares[r][c]==kingPiece) {kingRow=r; kingCol=c;} } }
             
               if(is_square_attacked(board, kingRow, kingCol, false)) return -MATE_SCORE + ply; // checkmated: delay if possible
             else return 0; // Stalemate
        }
        return maxEval;

    } else {
        int minEval = 1000000000;
        for (Move& move : possibleMoves) {
            
            board.makeMove(move);
            history.push_back(position_key(board));
            

            int kingPiece = -6;
            int kingRow = -1, kingCol = -1;
            for(int r=0; r<8; r++) { for(int c=0; c<8; c++) { if(board.squares[r][c]==kingPiece) {kingRow=r; kingCol=c;} } }
            
            // After a black move, the black king must not be attacked by white.
            if (is_square_attacked(board, kingRow, kingCol, true)) {
                history.pop_back();
                board.unmakeMove(move);
                continue;
            }
            
            legalMoveCount++;
            
            int eval = minimax(board, depth - 1, alpha, beta, ply + 1, history);
            history.pop_back();
            board.unmakeMove(move);
            
            minEval = std::min(minEval, eval);
            beta = std::min(beta, eval);
            if (beta <= alpha) break; // Alpha Cut-off
        }
        
        if (legalMoveCount == 0) {
             int kingPiece = -6;
             int kingRow = -1, kingCol = -1;
             for(int r=0; r<8; r++) { for(int c=0; c<8; c++) { if(board.squares[r][c]==kingPiece) {kingRow=r; kingCol=c;} } }
             
             if(is_square_attacked(board, kingRow, kingCol, true)) return MATE_SCORE - ply; // black checkmated: prefer faster
             else return 0;
        }
        return minEval;
    }
}

Move getBestMove(Board& board, int depth, const vector<uint64_t>& baseHistory) {
    bool isWhite = board.isWhiteTurn;
    vector<Move> possibleMoves = get_all_moves(board, isWhite);
    
    std::sort(possibleMoves.begin(), possibleMoves.end(), [&](const Move& a, const Move& b) {
        return scoreMove(board, a) > scoreMove(board, b);
    });
    
    Move bestMove;
    int bestValue = isWhite ? -2000000000 : 2000000000;
    if(!possibleMoves.empty()) bestMove = possibleMoves[0]; 

    vector<uint64_t> history = baseHistory;
    if (history.empty()) history.push_back(position_key(board));

    for (Move move : possibleMoves) {
        board.makeMove(move);
        history.push_back(position_key(board));
        
        int kingPiece = isWhite ? 6 : -6;
        int kingRow = -1, kingCol = -1;
        for(int r=0; r<8; r++) { for(int c=0; c<8; c++) { if(board.squares[r][c]==kingPiece) {kingRow=r; kingCol=c;} } }
        
        if (is_square_attacked(board, kingRow, kingCol, !isWhite)) {
            history.pop_back();
            board.unmakeMove(move);
            continue;
        }

        int val = minimax(board, depth - 1, -2000000000, 2000000000, 1, history);
        history.pop_back();
        board.unmakeMove(move);
        
        if (isWhite) {
            if (val > bestValue) { bestValue = val; bestMove = move; }
        } else {
            if (val < bestValue) { bestValue = val; bestMove = move; }
        }
    }
    return bestMove;
}


int main() {
    Board board;
    vector<uint64_t> gameHistory;
    gameHistory.reserve(512);
    string line;

    while (std::getline(std::cin, line)) {
        
        if (line == "uci") {
            cout << "id name SoloBot" << endl;
            cout << "id author xsolod3v" << endl;
            cout << "uciok" << endl;
        }
        
        // 2. "isready" gelirse
        else if (line == "isready") {
            cout << "readyok" << endl;
        }
        
        // 3. POZİSYON GÜNCELLEME (Örn: "position startpos moves e2e4 e7e5")
        else if (line.substr(0, 8) == "position") {
            board.resetBoard(); // Tahtayı sıfırla
            gameHistory.clear();
            gameHistory.push_back(position_key(board));
            
            size_t movesPos = line.find("moves");
            if (movesPos != string::npos) {

                string movesStr = line.substr(movesPos + 6);
                stringstream ss(movesStr);
                string moveToken;
                
                while (ss >> moveToken) {
                    Move m = uci_to_move(moveToken);
                    board.makeMove(m);
                    gameHistory.push_back(position_key(board));
                }
            }
        }
        
        else if (line.substr(0, 2) == "go") {
            int depth = 4;
            {
                stringstream ss(line);
                string token;
                ss >> token; // "go"
                while (ss >> token) {
                    if (token == "depth") {
                        int parsed = 0;
                        if (ss >> parsed) depth = parsed;
                    }
                }
            }

            Move best = getBestMove(board, depth, gameHistory);
            
            cout << "bestmove " << columns[best.fromCol] << (8 - best.fromRow) 
                 << columns[best.toCol] << (8 - best.toRow);
            
            if (best.promotion != 0) {
                switch (abs_int(best.promotion)) {
                    case queen:
                        cout << 'q';
                        break;
                    case rook:
                        cout << 'r';
                        break;
                    case bishop:
                        cout << 'b';
                        break;
                    case knight:
                        cout << 'n';
                        break;
                }
            }
            cout << endl;
        }
        
        else if (line == "quit") {
            break;
        }
    }
    return 0;
}