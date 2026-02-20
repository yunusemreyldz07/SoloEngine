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
inline constexpr int VALUE_INF = 2000000000;
inline constexpr int VALUE_NONE = -200000;

// Move ordering scores
inline constexpr int SCORE_TT_MOVE      = 1000000;
inline constexpr int SCORE_GOOD_CAPTURE = 1000000;
inline constexpr int SCORE_BAD_CAPTURE  = -100000;
inline constexpr int SCORE_PROMO_QUEEN  = 90000;
inline constexpr int SCORE_PROMO_ROOK   = 80000;
inline constexpr int SCORE_PROMO_BISHOP = -70000;
inline constexpr int SCORE_PROMO_KNIGHT = -60000;

inline constexpr int piece_type(int piece) {
    return (piece == 0) ? 0 : ((piece - 1) % 6) + 1;
}

inline constexpr int piece_color(int piece) {
    return (piece >= 7) ? BLACK : WHITE;
}

inline constexpr int make_piece(int type, int color) {
    return (color == WHITE) ? type : type + 6;
}

inline constexpr int other_color(int c) { return c ^ 1; }

extern int pieces_on_board[14];

// Move representation: 16 bits total
// Bits 0-5: from square (0-63)
// Bits 6-11: to square (0-63)
// Bits 12-13: Promoted piece type (0=none/knight, 1=bishop, 2=rook, 3=queen)
// Bits 14-15: flags (00=normal, 01=en passant, 10=castling, 11=capture)
using Move = uint16_t;

inline constexpr uint8_t MOVE_FLAG_NORMAL     = 0;
inline constexpr uint8_t MOVE_FLAG_EN_PASSANT = 1;
inline constexpr uint8_t MOVE_FLAG_CASTLING   = 2;
inline constexpr uint8_t MOVE_FLAG_CAPTURE    = 3;

inline constexpr uint8_t CASTLE_WHITE_K = 1;
inline constexpr uint8_t CASTLE_WHITE_Q = 2;
inline constexpr uint8_t CASTLE_BLACK_K = 4;
inline constexpr uint8_t CASTLE_BLACK_Q = 8;

inline int from_sq(Move m) { return m & 0x3F; }
inline int to_sq(Move m) { return (m >> 6) & 0x3F; }
inline int promotion_code(Move m) { return (m >> 12) & 0x3; }
inline int flags(Move m) { return (m >> 14) & 0x3; }

inline int promo_code_from_piece_type(int pt) {
    switch (pt) {
        case KNIGHT: return 0;
        case BISHOP: return 1;
        case ROOK: return 2;
        case QUEEN: return 3;
        default: return 0;
    }
}

inline int piece_type_from_promo_code(int code) {
    switch (code & 0x3) {
        case 0: return KNIGHT;
        case 1: return BISHOP;
        case 2: return ROOK;
        case 3: return QUEEN;
        default: return KNIGHT;
    }
}

inline Move make_move(int from, int to, int promoPieceType = 0, int moveFlags = MOVE_FLAG_NORMAL) {
    const int promo = promo_code_from_piece_type(promoPieceType);
    return static_cast<Move>((from & 0x3F) | ((to & 0x3F) << 6) | ((promo & 0x3) << 12) | ((moveFlags & 0x3) << 14));
}

inline bool is_en_passant(Move m) { return flags(m) == MOVE_FLAG_EN_PASSANT; }
inline bool is_castling(Move m) { return flags(m) == MOVE_FLAG_CASTLING; }
inline bool is_capture(Move m) { return flags(m) == MOVE_FLAG_CAPTURE || flags(m) == MOVE_FLAG_EN_PASSANT; }
inline bool moves_equal(Move a, Move b) { return a == b; }

struct Board;

struct UndoState {
    Move move = 0;
    int8_t movedPiece = 0;
    int8_t capturedPiece = 0;
    int8_t captureSquare = -1;
    int8_t rookFrom = -1;
    int8_t rookTo = -1;
    bool wasEnPassant = false;
    bool wasCastling = false;
    uint8_t prevCastling = 0;
    int8_t prevEnPassant = -1;
    int8_t prevHalfMoveClock = 0;
    uint64_t prevHash = 0;
};

struct Board {
    Bitboard piece[6];
    Bitboard color[2];
    uint64_t hash;

    uint8_t castling;
    int8_t enPassant; // square index [0..63], -1 if none
    uint8_t stm;      // side to move: 0 = white, 1 = black

    int8_t mailbox[64];
    int8_t halfMoveClock;

    std::vector<UndoState> undoStack;

    Board();
    void reset();
    void loadFEN(const std::string& fen);
    void makeMove(Move move);
    void unmakeMove(Move move);
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

inline int piece(const Board& board, Move m) {
    return board.mailbox[from_sq(m)];
}

inline bool is_promotion(const Board& board, Move m) {
    const int moving = piece(board, m);
    if (piece_type(moving) != PAWN) return false;
    const int to = to_sq(m);
    const int toRank = to / 8;
    return (piece_color(moving) == WHITE) ? (toRank == 7) : (toRank == 0);
}

inline int promotionPiece(Move m) {
    return piece_type_from_promo_code(promotion_code(m));
}

inline int promotion_piece(const Board& board, Move m) {
    return is_promotion(board, m) ? piece_type_from_promo_code(promotion_code(m)) : 0;
}

inline bool is_quiet(Move m) {
    return flags(m) == MOVE_FLAG_NORMAL && promotion_code(m) == 0;
}

inline bool is_quiet(const Board& board, Move m) {
    return !is_capture(m) && !is_promotion(board, m);
}

inline bool white_to_move(const Board& b) { return b.stm == WHITE; }
inline int side_to_move(const Board& b) { return b.stm; }
inline int opponent(const Board& b) { return b.stm ^ 1; }
inline bool can_castle(const Board& b, uint8_t right) { return (b.castling & right) != 0; }

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
int staticExchangeEvaluation(const Board& board, const Move& move, int threshold);

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
bool is_threefold_repetition(const std::vector<uint64_t>& positionHistory);

inline bool is_fifty_move_draw(const Board& board) {
    return board.halfMoveClock >= 100;
}

bool is_insufficient_material(const Board& board);

enum TTFlag {
    EXACT,
    ALPHA,
    BETA
};

class TranspositionTable {
public:
    TranspositionTable() : table(nullptr), size(0) {}
    ~TranspositionTable() { delete[] table; }

    void resize(int mbSize);
    void clear();
    void store(uint64_t hash, int score, int depth, TTFlag flag, const Move& bestMove);

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
