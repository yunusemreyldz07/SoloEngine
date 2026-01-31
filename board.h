#ifndef BOARD_H
#define BOARD_H

#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

#include "types.h"

extern char columns[];

// Piece constants
// Convention: empty = 0, white pieces = 1-6, black pieces = 7-12
// Piece type = ((piece - 1) % 6) + 1 for non-empty pieces
// Color = (piece >= 7) ? BLACK : WHITE for non-empty pieces
inline constexpr int EMPTY = 0;

// Colors
inline constexpr int WHITE = 0;
inline constexpr int BLACK = 1;

// White pieces (1-6)
inline constexpr int W_PAWN   = 1;
inline constexpr int W_KNIGHT = 2;
inline constexpr int W_BISHOP = 3;
inline constexpr int W_ROOK   = 4;
inline constexpr int W_QUEEN  = 5;
inline constexpr int W_KING   = 6;

// Black pieces (7-12)
inline constexpr int B_PAWN   = 7;
inline constexpr int B_KNIGHT = 8;
inline constexpr int B_BISHOP = 9;
inline constexpr int B_ROOK   = 10;
inline constexpr int B_QUEEN  = 11;
inline constexpr int B_KING   = 12;

// Piece types (for indexing, color-agnostic)
inline constexpr int PAWN   = 1;
inline constexpr int KNIGHT = 2;
inline constexpr int BISHOP = 3;
inline constexpr int ROOK   = 4;
inline constexpr int QUEEN  = 5;
inline constexpr int KING   = 6;

// Search constants
inline constexpr int MAX_PLY = 128;
inline constexpr int MATE_SCORE = 100000;
inline constexpr int VALUE_INF = 2000000000;   // Infinite score for alpha-beta bounds
inline constexpr int VALUE_NONE = -200000;     // Initial value before any move is searched

// Move ordering scores
inline constexpr int SCORE_TT_MOVE      = 1000000;
inline constexpr int SCORE_GOOD_CAPTURE = 1000000;
inline constexpr int SCORE_BAD_CAPTURE  = -100000;
inline constexpr int SCORE_KILLER_1     = 8000;
inline constexpr int SCORE_KILLER_2     = 7000;
inline constexpr int SCORE_PROMO_QUEEN  = 90000;
inline constexpr int SCORE_PROMO_ROOK   = 80000;
inline constexpr int SCORE_PROMO_BISHOP = -70000;
inline constexpr int SCORE_PROMO_KNIGHT = -60000;

// Helper functions for new piece encoding
inline constexpr int piece_type(int piece) {
    return (piece == 0) ? 0 : ((piece - 1) % 6) + 1;
}

inline constexpr int piece_color(int piece) {
    return (piece >= 7) ? BLACK : WHITE;
}

inline constexpr int make_piece(int type, int color) {
    return (color == WHITE) ? type : type + 6;
}

// Color helper
inline constexpr int other_color(int c) { return c ^ 1; }

extern int pieces_on_board[14]; // Simplified piece count for endgame detection (2 knights, 2 bishops, 2 rooks, 1 queen per side)

struct Move {
    int fromRow, fromCol;
    int toRow, toCol;
    int capturedPiece;
    int promotion;
    int pieceType;  // Moving piece type (1-6: pawn-king)

    bool prevW_KingSide;
    bool prevW_QueenSide;
    bool prevB_KingSide;
    bool prevB_QueenSide;

    int prevEnPassantCol;

    bool isEnPassant;
    bool isCastling;

    Move();

    // Helper methods for square-based access
    int from_sq() const { return (7 - fromRow) * 8 + fromCol; }
    int to_sq() const { return (7 - toRow) * 8 + toCol; }
};

// Compare two moves for equality (from/to squares and promotion)
inline bool moves_equal(const Move& a, const Move& b) {
    return a.fromRow == b.fromRow && a.fromCol == b.fromCol &&
           a.toRow == b.toRow && a.toCol == b.toCol &&
           a.promotion == b.promotion;
}

// Move type helpers
inline bool is_quiet(const Move& m) {
    return m.capturedPiece == 0 && !m.isEnPassant && m.promotion == 0;
}

inline bool is_capture(const Move& m) {
    return m.capturedPiece != 0 || m.isEnPassant;
}

class Board {
public:
    int pieces_otb[14];
    bool isWhiteTurn;

    Bitboard piece[6];
    Bitboard color[2];

    int mailbox[64]; // Redundant mailbox for O(1) piece lookups
    uint64_t currentHash; // Incremental Zobrist hash of the current position
    
    std::vector<Move> moveHistory; // History of moves for continuation history

    bool whiteCanCastleKingSide;
    bool whiteCanCastleQueenSide;
    bool blackCanCastleKingSide;
    bool blackCanCastleQueenSide;

    int whiteKingRow, whiteKingCol;
    int blackKingRow, blackKingCol;
    int enPassantCol;

    Board();
    void loadFromFEN(const std::string& fen);
    void resetBoard();
    void makeMove(Move& move);
    void unmakeMove(Move& move);
};

inline int row_col_to_sq(int row, int col) {
    return (7 - row) * 8 + col;
}

inline int sq_to_row(int sq) {
    return 7 - (sq / 8);
}

inline int sq_to_col(int sq) {
    return sq % 8;
}

inline int piece_at_sq(const Board& board, int sq) {
    return board.mailbox[sq];
}

// Board state helpers
inline int side_to_move(const Board& b) { return b.isWhiteTurn ? WHITE : BLACK; }
inline int opponent(const Board& b) { return b.isWhiteTurn ? BLACK : WHITE; }

inline bool king_square(const Board& board, bool white, int& outRow, int& outCol) {
    Bitboard k = board.piece[KING - 1] & board.color[white ? WHITE : BLACK];
    if (!k) return false;
    int sq = lsb(k);
    outRow = sq_to_row(sq);
    outCol = sq_to_col(sq);
    return true;
}

// Move generation functions
std::vector<Move> get_all_moves(Board& board, bool isWhiteTurn = true);
std::vector<Move> get_capture_moves(const Board& board);

// Attack detection
bool is_square_attacked(const Board& board, int row, int col, bool isWhiteAttacker);
int see_exchange(const Board& board, const Move& move);
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

class TranspositionTable {
public:
    TranspositionTable() : table(nullptr), size(0) {}
    ~TranspositionTable() { delete[] table; }

    // Resize the table to given size in MB (count = bytes / sizeof(entry)).
    void resize(int mbSize);
    void clear();

    // Lockless + thread-safe: write data first, then publish key with release semantics.
    void store(uint64_t hash, int score, int depth, TTFlag flag, const Move& bestMove);

    // Backward-compatible overload (older call sites passing an int)
    void store(uint64_t hash, int score, int depth, int flag, const Move& bestMove) {
        store(hash, score, depth, static_cast<TTFlag>(flag), bestMove);
    }

    bool probe(uint64_t key, int& outScore, int& outDepth, TTFlag& outFlag, Move& outMove) const;
    size_t entryCount() const { return size; }

private:
    struct TTAtomicEntry {
        std::atomic<uint64_t> key{0};
        std::atomic<uint64_t> data{0};
    };

    TTAtomicEntry* table;
    size_t size;

    static uint16_t packMove(const Move& m);
    static Move unpackMove(uint16_t packed);
    static uint64_t packData(int score, int depth, TTFlag flag, uint16_t packedMove);
    static void unpackData(uint64_t data, int& score, int& depth, TTFlag& flag, uint16_t& packedMove);
};

extern TranspositionTable globalTT;

#endif
