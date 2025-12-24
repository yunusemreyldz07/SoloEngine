#ifndef BOARD_H
#define BOARD_H

#include <vector>
#include <cstdint>
#include <string>
#include <unordered_map>

// Piece constants
// Convention: empty = 0, white pieces are positive, black pieces are negative.
inline constexpr int empty_sqr = 0;
inline constexpr int pawn      = 1;
inline constexpr int knight    = 2;
inline constexpr int bishop    = 3;
inline constexpr int rook      = 4;
inline constexpr int queen     = 5;
inline constexpr int king      = 6;

extern int pieces_on_board[14]; // Simplified piece count for endgame detection (2 knights, 2 bishops, 2 rooks, 1 queen per side)

// Move directions
extern int knight_moves[8][2];
extern int king_moves[8][2];
extern int bishop_directions[4][2];
extern int rook_directions[4][2];
extern int queen_directions[8][2];

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

    Move();
};

class Board {
public:
    int pieces_otb[14];
    int squares[8][8];
    bool isWhiteTurn;
    
    bool whiteCanCastleKingSide;
    bool whiteCanCastleQueenSide;
    bool blackCanCastleKingSide;
    bool blackCanCastleQueenSide;
    
    int whiteKingRow, whiteKingCol;
    int blackKingRow, blackKingCol;
    int enPassantCol;

    Board();
    void resetBoard();
    void makeMove(Move& move);
    void unmakeMove(Move& move);
};

// Move generation functions
std::vector<Move> generate_pawn_moves(const Board& board, int row, int col);
std::vector<Move> generate_knight_moves(const Board& board, int row, int col);
std::vector<Move> generate_bishop_moves(const Board& board, int row, int col);
std::vector<Move> generate_rook_moves(const Board& board, int row, int col);
std::vector<Move> generate_queen_moves(const Board& board, int row, int col);
std::vector<Move> generate_king_moves(const Board& board, int row, int col);

std::vector<Move> get_all_moves(const Board& board, bool isWhiteTurn = true);
std::vector<Move> get_capture_moves(const Board& board);

// Attack detection
bool is_square_attacked(const Board& board, int row, int col, bool isWhiteAttacker);

// Utility functions
void printBoard(const Board& board);
Move uci_to_move(const std::string& uci);

// Zobrist hashing
struct Zobrist {
    uint64_t piece[12][64]{};
    uint64_t castling[16]{};
    uint64_t epFile[9]{};
    uint64_t side{};

    static uint64_t splitmix64(uint64_t& x);
    Zobrist();
};

const Zobrist& zobrist();
int piece_to_zobrist_index(int piece);
uint64_t position_key(const Board& board);
bool is_threefold_repetition(const std::vector<uint64_t>& history);

enum TTFlag {
    EXACT,      // Exact score
    ALPHA,      // Upper bound (fail-low)
    BETA        // Lower bound (fail-high)
};
/*
By the way, the concept of these flags is to indicate the type of score stored in the transposition table entry.
EXACT means the score is precise, ALPHA indicates the score is an upper bound (the true score is less than or equal to this), and BETA indicates the score is a lower bound (the true score is greater than or equal to this).
It is like an exam. Alpha is when you know you did at most 70 points (you could have done less), Beta is when you know you did at least 70 points (you could have done more), and Exact is when you know you did exactly 70 points.
*/

struct TTEntry {
    uint64_t hash; // Position hash key
    int depth; // Search depth at which this entry was stored
    int score; // Evaluation score
    TTFlag flag; // Type of score
    Move bestMove; // Best move found
};
TTEntry* probeTranspositionTable(uint64_t key, std::unordered_map<uint64_t, TTEntry>& table);
void storeInTT(uint64_t key, int score, int depth, TTFlag flag, Move bestMove, std::unordered_map<uint64_t, TTEntry>& table);
extern std::unordered_map<uint64_t, TTEntry> transpositionTable;
Move isInTranspositionTable(uint64_t key, const std::unordered_map<uint64_t, Move>& table);

#endif