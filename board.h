#ifndef BOARD_H
#define BOARD_H

#include "bitboard.h"
#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

extern char columns[];

inline constexpr int empty_sqr = 0;
inline constexpr int pawn = 1;
inline constexpr int knight = 2;
inline constexpr int bishop = 3;
inline constexpr int rook = 4;
inline constexpr int queen = 5;
inline constexpr int king = 6;

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

  Bitboard byColor[2];
  Bitboard byPiece[7];

  Bitboard occupancy() const { return byColor[0] | byColor[1]; }
  Bitboard pieces(int color) const { return byColor[color]; }
  Bitboard pieces(int color, int pieceType) const {
    return byColor[color] & byPiece[pieceType];
  }

  Board();
  void loadFromFEN(const std::string &fen);
  void resetBoard();
  void makeMove(Move &move);
  void unmakeMove(Move &move);

  void updateBitboards();
  void setBit(int sq, int piece, int color);
  void clearBit(int sq, int piece, int color);
};

void generate_pawn_moves(const Board &board, int row, int col,
                         std::vector<Move> &moveList);
void generate_knight_moves(const Board &board, int row, int col,
                           std::vector<Move> &moveList);
void generate_bishop_moves(const Board &board, int row, int col,
                           std::vector<Move> &moveList);
void generate_rook_moves(const Board &board, int row, int col,
                         std::vector<Move> &moveList);
void generate_queen_moves(const Board &board, int row, int col,
                          std::vector<Move> &moveList);
void generate_king_moves(const Board &board, int row, int col,
                         std::vector<Move> &moveList);
std::vector<Move> get_all_moves(Board &board, bool isWhiteTurn = true);
std::vector<Move> get_capture_moves(const Board &board);

bool is_square_attacked(const Board &board, int row, int col,
                        bool isWhiteAttacker);
bool is_square_attacked_bb(const Board &board, int sq, bool byWhite);
int see_exchange(Board &board, Move &move);

void printBoard(const Board &board);
Move uci_to_move(const std::string &uci);

struct Zobrist {
  uint64_t piece[12][64]{};
  uint64_t castling[16]{};
  uint64_t epFile[9]{};
  uint64_t side{};

  static uint64_t splitmix64(uint64_t &x);
  Zobrist();
};

const Zobrist &zobrist();
int piece_to_zobrist_index(int piece);
uint64_t position_key(const Board &board);
bool is_threefold_repetition(const std::vector<uint64_t> &history);

enum TTFlag { EXACT, ALPHA, BETA };

class TranspositionTable {
public:
  TranspositionTable() : table(nullptr), size(0) {}
  ~TranspositionTable() { delete[] table; }

  void resize(int mbSize);
  void clear();

  void store(uint64_t hash, int score, int depth, TTFlag flag,
             const Move &bestMove);

  void store(uint64_t hash, int score, int depth, int flag,
             const Move &bestMove) {
    store(hash, score, depth, static_cast<TTFlag>(flag), bestMove);
  }

  bool probe(uint64_t key, int &outScore, int &outDepth, TTFlag &outFlag,
             Move &outMove) const;
  size_t entryCount() const { return size; }

private:
  struct TTAtomicEntry {
    std::atomic<uint64_t> key{0};
    std::atomic<uint64_t> data{0};
  };

  TTAtomicEntry *table;
  size_t size;

  static uint16_t packMove(const Move &m);
  static Move unpackMove(uint16_t packed);
  static uint64_t packData(int score, int depth, TTFlag flag,
                           uint16_t packedMove);
  static void unpackData(uint64_t data, int &score, int &depth, TTFlag &flag,
                         uint16_t &packedMove);
};

extern TranspositionTable globalTT;

#endif