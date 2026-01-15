#ifndef BOARD_H
#define BOARD_H

#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

#include "types.h"

extern char columns[];

// Piece constants
// Convention: empty = 0, white pieces are positive, black pieces are negative.
inline constexpr int empty_sqr = 0;
inline constexpr int pawn      = 1;
inline constexpr int knight    = 2;
inline constexpr int bishop    = 3;
inline constexpr int rook      = 4;
inline constexpr int queen     = 5;
inline constexpr int king      = 6;
inline constexpr int WHITE     = 0;
inline constexpr int BLACK     = 1;

extern int pieces_on_board[14]; // Simplified piece count for endgame detection (2 knights, 2 bishops, 2 rooks, 1 queen per side)

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
    bool isWhiteTurn;

    Bitboard piece[6];
    Bitboard color[2];

    int mailbox[64]; // Redundant mailbox for O(1) piece lookups
    uint64_t currentHash; // Incremental Zobrist hash of the current position

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

inline bool king_square(const Board& board, bool white, int& outRow, int& outCol) {
    Bitboard k = board.piece[king - 1] & board.color[white ? WHITE : BLACK];
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
