#ifndef BOARD_H
#define BOARD_H

#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

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
    void loadFromFEN(const std::string& fen);
    void resetBoard();
    void makeMove(Move& move);
    void unmakeMove(Move& move);
};

// Move generation functions
void generate_pawn_moves(const Board& board, int row, int col, std::vector<Move>& moveList);
void generate_knight_moves(const Board& board, int row, int col, std::vector<Move>& moveList);
void generate_bishop_moves(const Board& board, int row, int col, std::vector<Move>& moveList);
void generate_rook_moves(const Board& board, int row, int col, std::vector<Move>& moveList);
void generate_queen_moves(const Board& board, int row, int col, std::vector<Move>& moveList);
void generate_king_moves(const Board& board, int row, int col, std::vector<Move>& moveList);
std::vector<Move> get_all_moves(Board& board, bool isWhiteTurn = true);
std::vector<Move> get_capture_moves(const Board& board);

// Attack detection
bool is_square_attacked(const Board& board, int row, int col, bool isWhiteAttacker);
int see_exchange(Board& board, Move& move);
static int get_least_valuable_attacker(Board& board, int square, int bySide, int& attackerSq);
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