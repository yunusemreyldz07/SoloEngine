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
inline constexpr int16_t MATE_SCORE = 30000; // Score for checkmate positions, used in evaluation and search
inline constexpr int16_t VALUE_INF = 32000;   // Infinite score for alpha-beta bounds

// Move ordering scores
inline constexpr int SCORE_TT_MOVE      = 1000000;
inline constexpr int SCORE_GOOD_CAPTURE = 1000000;
inline constexpr int SCORE_BAD_CAPTURE  = -100000;
inline constexpr int SCORE_PROMO_QUEEN  = 90000;
inline constexpr int SCORE_PROMO_ROOK   = 80000;
inline constexpr int SCORE_PROMO_BISHOP = -70000;
inline constexpr int SCORE_PROMO_KNIGHT = -60000;

inline constexpr uint8_t CASTLE_WK = 1; // White Kingside (0001)
inline constexpr uint8_t CASTLE_WQ = 2; // White Queenside (0010)
inline constexpr uint8_t CASTLE_BK = 4; // Black Kingside (0100)
inline constexpr uint8_t CASTLE_BQ = 8; // Black Queenside (1000)

enum MoveFlags {
    FLAG_QUIET = 0,
    FLAG_DOUBLE_PAWN = 1,
    FLAG_CASTLE_KING = 2,
    FLAG_CASTLE_QUEEN = 3,
    FLAG_CAPTURE = 4,
    FLAG_EN_PASSANT = 5,
    FLAG_PROMO_KNIGHT = 8,
    FLAG_PROMO_BISHOP = 9,
    FLAG_PROMO_ROOK = 10,
    FLAG_PROMO_QUEEN = 11,
    FLAG_PROMO_KNIGHT_CAPTURE = 12,
    FLAG_PROMO_BISHOP_CAPTURE = 13,
    FLAG_PROMO_ROOK_CAPTURE = 14,
    FLAG_PROMO_QUEEN_CAPTURE = 15
};    

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

// Move representation: 16 bits total
// Bits 0-5: from square (0-63)
// Bits 6-11: to square (0-63)
// Bits 12-15: flags (4 bit)

// --- FLAG LIST ---
// 0000 (0) = No flag
// 0001 (1) = Double Pawn Push
// 0010 (2) = Kingside Castle
// 0011 (3) = Queenside Castle
// 0100 (4) = Normal Capture
// 0101 (5) = EP Capture
// 1000 (8) = Promo Knight
// 1001 (9) = Promo Bishop
// 1010 (10)= Promo Rook
// 1011 (11)= Promo Queen
// 1100 (12)= Promo Knight + Capture
// 1101 (13)= Promo Bishop + Capture
// 1110 (14)= Promo Rook + Capture
// 1111 (15)= Promo Queen + Capture

// The reason why I chose this encoding:
// In all promotion types, 3rd bit is always 1. 
// And in all capture types, 2nd bit is always 1. 
// This allows for easy detection of quiet moves vs captures vs promotions with simple bit checks

using Move = uint16_t;

inline int move_flags(Move m) { return (m >> 12) & 0xF; } // Last 4 bits

inline int get_promotion_type(Move m) {
    // 8 = 1000 in binary, so we check if the 3rd bit is set to determine if it's a promotion
    // If it's a promotion, the type is determined by the last 2 (0011 -> 3 since 2^1 + 2^0) bits of the flags
    int promo = (move_flags(m) & 8) != 0 ? (move_flags(m) & 3) : -1;
    if (promo != -1) {
        switch (promo) {
            case 0: return KNIGHT; // 00
            case 1: return BISHOP; // 01
            case 2: return ROOK;   // 10
            case 3: return QUEEN;  // 11
        }
    }
    return -1; // Not a promotion
}

// Compare two moves for equality (from/to squares and promotion)
inline bool moves_equal(const Move& a, const Move& b) {
    return a == b;
}

// Move type helpers
inline bool is_quiet(const Move& m) {
    return move_flags(m) < 4; // No capture, no promotion 
}

inline bool is_capture(Move m) {
    // If the 2nd bit of flags is 1, it's definitely a capture 
    return (move_flags(m) & 4) != 0; 
}

inline bool is_promotion(Move m) {
    // If the 3rd bit of flags is 1, it's definitely a promotion (1000 which is 8 in decimal)
    return (move_flags(m) & 8) != 0; 
}

inline int move_from(Move m) { return m & 0x3F; } // First 6 bits
inline int move_to(Move m) { return (m >> 6) & 0x3F; } // Next 6 bits

inline Move create_move(int from, int to, int flags) {
    return Move((from & 63) | ((to & 63) << 6) | ((flags & 15) << 12)); 
}

struct UndoState {
    int capturedPiece;    // We gotta remember the piece we took so we can put it back
    uint8_t castling;     // Castling rights before the move (4 bits: KQkq)
    int8_t enPassant;     // EP column copy (-1 or 0-7)
    int8_t halfMoveClock; // 50 move rule counter before the move
    uint64_t hash;         // Position hash before the move (for repetition detection)
};

struct Board {
    Bitboard piece[6];
    Bitboard color[2];
    uint64_t hash;

    uint8_t castling;
    int8_t enPassant;
    uint8_t stm; // side to move: 0 = white, 1 = black

    std::vector<Move> moveHistory;
    std::vector<UndoState> undoStack;

    int8_t mailbox[64];
    int8_t halfMoveClock;

    Board();
    void reset();
    void loadFEN(const std::string& fen);
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
inline int side_to_move(const Board& b) { return b.stm; }
inline int opponent(const Board& b) { return other_color(b.stm); }

inline bool king_square(const Board& board, bool white, int& outRow, int& outCol) {
    Bitboard k = board.piece[KING - 1] & board.color[white ? WHITE : BLACK];
    if (!k) return false;
    int sq = lsb(k);
    outRow = sq_to_row(sq);
    outCol = sq_to_col(sq);
    return true;
}

// Move generation functions
void get_all_moves(Board& board, Move moves[], int& moveCount);
void get_capture_moves(const Board& board, Move moves[], int& moveCount);

// Attack detection
bool is_square_attacked(const Board& board, int row, int col, bool isWhiteAttacker);
int see_exchange(const Board& board, const Move& move);
int staticExchangeEvaluation(const Board& board, const Move& move, int threshold);
// Utility functions
void printBoard(const Board& board);
Move uci_to_move(const std::string& uci, const Board& board);

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

// Draw detection
inline bool is_fifty_move_draw(const Board& board) {
    return board.halfMoveClock >= 100;  // 100 half-moves = 50 full moves
}

// Insufficient material detection
// Returns true only for positions where checkmate is IMPOSSIBLE
// Conservative: K vs K, K+B vs K, K+N vs K
// Does NOT include K+B+N vs K (can mate) or K+B vs K+B same color (can mate in corners with help)
bool is_insufficient_material(const Board& board);

#endif
