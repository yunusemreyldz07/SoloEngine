#ifndef BOARD_H
#define BOARD_H
#include "bitboard.h"
#include <atomic>
#include <string>
#include <vector>

enum Piece { PAWN = 1, KNIGHT = 2, BISHOP = 3, ROOK = 4, QUEEN = 5, KING = 6 };

struct Move {
  int8_t fromRow, fromCol, toRow, toCol;
  int8_t capturedPiece, promotion;
  bool prevWK, prevWQ, prevBK, prevBQ;
  int8_t prevEP;
  bool isEP, isCastle;
  int score;
  Move()
      : fromRow(0), fromCol(0), toRow(0), toCol(0), capturedPiece(0),
        promotion(0), prevWK(0), prevWQ(0), prevBK(0), prevBQ(0), prevEP(-1),
        isEP(0), isCastle(0), score(0) {}
};

class Board {
public:
  int8_t squares[64];
  Bitboard byColor[2], byPiece[7];
  bool castleWK, castleWQ, castleBK, castleBQ, whiteTurn;
  int8_t epCol, wKingSq, bKingSq;
  Board();
  void reset();
  void loadFEN(const std::string &fen);
  void makeMove(Move &m);
  void unmakeMove(Move &m);
  void updateBitboards();
  inline Bitboard occupancy() const { return byColor[0] | byColor[1]; }
  inline Bitboard pieces(int color, int piece) const {
    return byColor[color] & byPiece[piece];
  }
  inline bool isInsufficientMaterial() const {
    return !(byPiece[PAWN] | byPiece[ROOK] | byPiece[QUEEN]) &&
           popcount(occupancy()) <= 3;
  }
};

std::vector<Move> generateMoves(const Board &b);
bool isAttacked(const Board &b, int sq, bool byWhite);
Move parseUCI(const std::string &uci);
std::string moveToUCI(const Move &m);

struct Zobrist {
  uint64_t piece[12][64], castling[16], ep[9], side;
  Zobrist();
};
uint64_t positionKey(const Board &b);
bool isRepetition(const std::vector<uint64_t> &hist);

enum class TTFlag { EXACT, LOWER, UPPER };
struct TTEntry {
  std::atomic<uint64_t> key, data;
};
struct TT {
  TTEntry *table = nullptr;
  size_t size = 0;
  void resize(int mb);
  void clear();
  bool probe(uint64_t k, int &score, int &depth, TTFlag &flag, Move &m);
  void store(uint64_t k, int score, int depth, TTFlag flag, const Move &m);
};
extern TT tt;

#endif