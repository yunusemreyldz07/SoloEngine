#include "board.h"
#include <cstdlib>
#include <sstream>

TT tt;

Board::Board() { reset(); }

void Board::reset() {
  const int8_t init[64] = {
      -ROOK, -KNIGHT, -BISHOP, -QUEEN, -KING, -BISHOP, -KNIGHT, -ROOK,
      -PAWN, -PAWN,   -PAWN,   -PAWN,  -PAWN, -PAWN,   -PAWN,   -PAWN,
      0,     0,       0,       0,      0,     0,       0,       0,
      0,     0,       0,       0,      0,     0,       0,       0,
      0,     0,       0,       0,      0,     0,       0,       0,
      0,     0,       0,       0,      0,     0,       0,       0,
      PAWN,  PAWN,    PAWN,    PAWN,   PAWN,  PAWN,    PAWN,    PAWN,
      ROOK,  KNIGHT,  BISHOP,  QUEEN,  KING,  BISHOP,  KNIGHT,  ROOK};
  for (int i = 0; i < 64; i++)
    squares[i] = init[i];
  castleWK = castleWQ = castleBK = castleBQ = true;
  whiteTurn = true;
  epCol = -1;
  wKingSq = 60;
  bKingSq = 4;
  updateBitboards();
}

void Board::updateBitboards() {
  for (int i = 0; i < 2; i++)
    byColor[i] = 0;
  for (int i = 0; i < 7; i++)
    byPiece[i] = 0;
  for (int sq = 0; sq < 64; sq++) {
    int p = squares[sq];
    if (p) {
      int c = p > 0 ? 0 : 1;
      byColor[c] |= square_bb(sq);
      byPiece[std::abs(p)] |= square_bb(sq);
    }
  }
}

void Board::loadFEN(const std::string &fen) {
  for (int i = 0; i < 64; i++)
    squares[i] = 0;
  std::istringstream ss(fen);
  std::string pos, turn, cast, ep;
  ss >> pos >> turn >> cast >> ep;
  int sq = 0;
  for (char c : pos) {
    if (c == '/')
      continue;
    if (c >= '1' && c <= '8') {
      sq += c - '0';
      continue;
    }
    int p = 0;
    switch (tolower(c)) {
    case 'p':
      p = PAWN;
      break;
    case 'n':
      p = KNIGHT;
      break;
    case 'b':
      p = BISHOP;
      break;
    case 'r':
      p = ROOK;
      break;
    case 'q':
      p = QUEEN;
      break;
    case 'k':
      p = KING;
      break;
    }
    if (islower(c))
      p = -p;
    squares[sq] = p;
    if (p == KING)
      wKingSq = sq;
    else if (p == -KING)
      bKingSq = sq;
    sq++;
  }
  whiteTurn = (turn == "w");
  castleWK = cast.find('K') != std::string::npos;
  castleWQ = cast.find('Q') != std::string::npos;
  castleBK = cast.find('k') != std::string::npos;
  castleBQ = cast.find('q') != std::string::npos;
  epCol = (ep != "-" && ep.length() > 0) ? ep[0] - 'a' : -1;
  updateBitboards();
}

void Board::makeMove(Move &m) {
  int from = m.fromRow * 8 + m.fromCol, to = m.toRow * 8 + m.toCol;
  int p = squares[from];
  squares[from] = 0;
  squares[to] = p;
  if (m.isEP)
    squares[m.fromRow * 8 + m.toCol] = 0;
  if (m.isCastle) {
    if (m.toCol > m.fromCol) {
      squares[m.toRow * 8 + 5] = squares[m.toRow * 8 + 7];
      squares[m.toRow * 8 + 7] = 0;
    } else {
      squares[m.toRow * 8 + 3] = squares[m.toRow * 8 + 0];
      squares[m.toRow * 8 + 0] = 0;
    }
  }
  if (m.promotion)
    squares[to] = p > 0 ? m.promotion : -m.promotion;
  epCol = (std::abs(p) == PAWN && std::abs(m.fromRow - m.toRow) == 2) ? m.toCol
                                                                      : -1;
  whiteTurn = !whiteTurn;
  if (p == KING) {
    castleWK = castleWQ = false;
    wKingSq = to;
  } else if (p == -KING) {
    castleBK = castleBQ = false;
    bKingSq = to;
  } else if (p == ROOK) {
    if (from == 63)
      castleWK = false;
    else if (from == 56)
      castleWQ = false;
  } else if (p == -ROOK) {
    if (from == 7)
      castleBK = false;
    else if (from == 0)
      castleBQ = false;
  }
  if (m.capturedPiece == ROOK) {
    if (to == 63)
      castleWK = false;
    else if (to == 56)
      castleWQ = false;
  } else if (m.capturedPiece == -ROOK) {
    if (to == 7)
      castleBK = false;
    else if (to == 0)
      castleBQ = false;
  }
  updateBitboards();
}

void Board::unmakeMove(Move &m) {
  whiteTurn = !whiteTurn;
  int from = m.fromRow * 8 + m.fromCol, to = m.toRow * 8 + m.toCol;
  int p = squares[to];
  if (m.promotion)
    p = whiteTurn ? PAWN : -PAWN;
  squares[to] = 0;
  squares[from] = p;
  if (m.isEP)
    squares[m.fromRow * 8 + m.toCol] = whiteTurn ? -PAWN : PAWN;
  else if (m.capturedPiece)
    squares[to] = m.capturedPiece;
  if (m.isCastle) {
    if (m.toCol > m.fromCol) {
      squares[m.toRow * 8 + 7] = squares[m.toRow * 8 + 5];
      squares[m.toRow * 8 + 5] = 0;
    } else {
      squares[m.toRow * 8 + 0] = squares[m.toRow * 8 + 3];
      squares[m.toRow * 8 + 3] = 0;
    }
  }
  castleWK = m.prevWK;
  castleWQ = m.prevWQ;
  castleBK = m.prevBK;
  castleBQ = m.prevBQ;
  epCol = m.prevEP;
  if (p == KING)
    wKingSq = from;
  else if (p == -KING)
    bKingSq = from;
  updateBitboards();
}

bool isAttacked(const Board &b, int sq, bool byWhite) {
  int side = byWhite ? 0 : 1;
  Bitboard occ = b.occupancy();
  if (knight_attacks(sq) & b.pieces(side, KNIGHT))
    return true;
  if (king_attacks(sq) & b.pieces(side, KING))
    return true;
  if (pawn_attacks(1 - side, sq) & b.pieces(side, PAWN))
    return true;
  if (bishop_attacks(sq, occ) &
      (b.pieces(side, BISHOP) | b.pieces(side, QUEEN)))
    return true;
  if (rook_attacks(sq, occ) & (b.pieces(side, ROOK) | b.pieces(side, QUEEN)))
    return true;
  return false;
}

static void addMove(const Board &b, int from, int to, std::vector<Move> &moves,
                    int promotion = 0, bool isEP = false,
                    bool isCastle = false) {
  Move m;
  m.fromRow = from >> 3;
  m.fromCol = from & 7;
  m.toRow = to >> 3;
  m.toCol = to & 7;
  m.capturedPiece = isEP ? 0 : b.squares[to];
  m.promotion = promotion;
  m.isEP = isEP;
  m.isCastle = isCastle;
  m.prevWK = b.castleWK;
  m.prevWQ = b.castleWQ;
  m.prevBK = b.castleBK;
  m.prevBQ = b.castleBQ;
  m.prevEP = b.epCol;
  moves.push_back(m);
}

static void addPawnMoves(const Board &b, int sq, std::vector<Move> &moves) {
  int r = sq >> 3, c = sq & 7;
  int p = b.squares[sq];
  int dir = p > 0 ? -1 : 1;
  int promoR = p > 0 ? 0 : 7;
  int startR = p > 0 ? 6 : 1;
  int nr = r + dir;
  if (nr >= 0 && nr < 8 && b.squares[nr * 8 + c] == 0) {
    int to = nr * 8 + c;
    if (nr == promoR) {
      for (int pr : {QUEEN, ROOK, BISHOP, KNIGHT})
        addMove(b, sq, to, moves, pr);
    } else {
      addMove(b, sq, to, moves);
      if (r == startR && b.squares[(r + 2 * dir) * 8 + c] == 0)
        addMove(b, sq, (r + 2 * dir) * 8 + c, moves);
    }
  }
  for (int dc : {-1, 1}) {
    int nc = c + dc;
    if (nc < 0 || nc >= 8 || nr < 0 || nr >= 8)
      continue;
    int to = nr * 8 + nc;
    int target = b.squares[to];
    bool canCap = (p > 0 && target < 0) || (p < 0 && target > 0);
    bool isEP = (nr == (p > 0 ? 2 : 5) && b.epCol == nc);
    if (canCap || isEP) {
      if (nr == promoR) {
        for (int pr : {QUEEN, ROOK, BISHOP, KNIGHT})
          addMove(b, sq, to, moves, pr, isEP);
      } else {
        addMove(b, sq, to, moves, 0, isEP);
      }
    }
  }
}

static void addPieceMoves(const Board &b, int sq, Bitboard att,
                          std::vector<Move> &moves) {
  int p = b.squares[sq];
  Bitboard friendly = p > 0 ? b.byColor[0] : b.byColor[1];
  att &= ~friendly;
  while (att) {
    int to = pop_lsb(att);
    addMove(b, sq, to, moves);
  }
}

static void addKingMoves(const Board &b, int sq, std::vector<Move> &moves) {
  addPieceMoves(b, sq, king_attacks(sq), moves);
  int p = b.squares[sq];
  bool isWhite = p > 0;
  int homeR = isWhite ? 7 : 0;
  int r = sq >> 3, c = sq & 7;
  if (r != homeR || c != 4)
    return;
  if (isAttacked(b, sq, !isWhite))
    return;
  if ((isWhite ? b.castleWK : b.castleBK) && b.squares[homeR * 8 + 5] == 0 &&
      b.squares[homeR * 8 + 6] == 0 &&
      b.squares[homeR * 8 + 7] == (isWhite ? ROOK : -ROOK)) {
    if (!isAttacked(b, homeR * 8 + 5, !isWhite) &&
        !isAttacked(b, homeR * 8 + 6, !isWhite)) {
      addMove(b, sq, homeR * 8 + 6, moves, 0, false, true);
    }
  }
  if ((isWhite ? b.castleWQ : b.castleBQ) && b.squares[homeR * 8 + 1] == 0 &&
      b.squares[homeR * 8 + 2] == 0 && b.squares[homeR * 8 + 3] == 0 &&
      b.squares[homeR * 8 + 0] == (isWhite ? ROOK : -ROOK)) {
    if (!isAttacked(b, homeR * 8 + 3, !isWhite) &&
        !isAttacked(b, homeR * 8 + 2, !isWhite)) {
      addMove(b, sq, homeR * 8 + 2, moves, 0, false, true);
    }
  }
}

std::vector<Move> generateMoves(const Board &b) {
  std::vector<Move> moves;
  moves.reserve(256);
  Bitboard occ = b.occupancy();
  Bitboard our = b.whiteTurn ? b.byColor[0] : b.byColor[1];
  while (our) {
    int sq = pop_lsb(our);
    int p = b.squares[sq];
    int pt = std::abs(p);
    switch (pt) {
    case PAWN:
      addPawnMoves(b, sq, moves);
      break;
    case KNIGHT:
      addPieceMoves(b, sq, knight_attacks(sq), moves);
      break;
    case BISHOP:
      addPieceMoves(b, sq, bishop_attacks(sq, occ), moves);
      break;
    case ROOK:
      addPieceMoves(b, sq, rook_attacks(sq, occ), moves);
      break;
    case QUEEN:
      addPieceMoves(b, sq, queen_attacks(sq, occ), moves);
      break;
    case KING:
      addKingMoves(b, sq, moves);
      break;
    }
  }
  return moves;
}

Move parseUCI(const std::string &uci) {
  Move m;
  if (uci.length() < 4)
    return m;
  m.fromCol = uci[0] - 'a';
  m.fromRow = 8 - (uci[1] - '0');
  m.toCol = uci[2] - 'a';
  m.toRow = 8 - (uci[3] - '0');
  if (uci.length() == 5) {
    switch (uci[4]) {
    case 'q':
      m.promotion = QUEEN;
      break;
    case 'r':
      m.promotion = ROOK;
      break;
    case 'b':
      m.promotion = BISHOP;
      break;
    case 'n':
      m.promotion = KNIGHT;
      break;
    }
  }
  return m;
}

std::string moveToUCI(const Move &m) {
  if (m.fromRow == 0 && m.fromCol == 0 && m.toRow == 0 && m.toCol == 0)
    return "0000";
  std::string s;
  s += (char)('a' + m.fromCol);
  s += (char)('8' - m.fromRow);
  s += (char)('a' + m.toCol);
  s += (char)('8' - m.toRow);
  if (m.promotion) {
    switch (m.promotion) {
    case QUEEN:
      s += 'q';
      break;
    case ROOK:
      s += 'r';
      break;
    case BISHOP:
      s += 'b';
      break;
    case KNIGHT:
      s += 'n';
      break;
    }
  }
  return s;
}

Zobrist::Zobrist() {
  uint64_t s = 0xDEADBEEF;
  auto rand = [&]() {
    s ^= s << 13;
    s ^= s >> 7;
    s ^= s << 17;
    return s;
  };
  for (int i = 0; i < 12; i++)
    for (int j = 0; j < 64; j++)
      piece[i][j] = rand();
  for (int i = 0; i < 16; i++)
    castling[i] = rand();
  for (int i = 0; i < 9; i++)
    ep[i] = rand();
  side = rand();
}

static const Zobrist &zob() {
  static Zobrist z;
  return z;
}

uint64_t positionKey(const Board &b) {
  uint64_t h = 0;
  for (int sq = 0; sq < 64; sq++) {
    int p = b.squares[sq];
    if (p) {
      int idx = (p > 0 ? 0 : 6) + std::abs(p) - 1;
      h ^= zob().piece[idx][sq];
    }
  }
  int c = (b.castleWK ? 1 : 0) | (b.castleWQ ? 2 : 0) | (b.castleBK ? 4 : 0) |
          (b.castleBQ ? 8 : 0);
  h ^= zob().castling[c];
  h ^= zob().ep[b.epCol >= 0 && b.epCol < 8 ? b.epCol : 8];
  if (b.whiteTurn)
    h ^= zob().side;
  return h;
}

bool isRepetition(const std::vector<uint64_t> &hist) {
  if (hist.size() < 4)
    return false;
  uint64_t cur = hist.back();
  int cnt = 0;
  for (int i = (int)hist.size() - 1; i >= 0; i--)
    if (hist[i] == cur && ++cnt >= 3)
      return true;
  return false;
}

void TT::resize(int mb) {
  delete[] table;
  table = nullptr;
  size = 0;
  if (mb <= 0)
    return;
  size = (size_t)mb * 1024 * 1024 / sizeof(TTEntry);
  if (size == 0)
    size = 1;
  table = new TTEntry[size];
  clear();
}

void TT::clear() {
  if (table)
    for (size_t i = 0; i < size; i++) {
      table[i].key.store(0);
      table[i].data.store(0);
    }
}

static uint32_t packMove(const Move &m) {
  return (m.fromRow * 8 + m.fromCol) | ((m.toRow * 8 + m.toCol) << 6) |
         (m.promotion << 12) | (m.isEP ? (1 << 16) : 0) |
         (m.isCastle ? (1 << 17) : 0);
}
static Move unpackMove(uint32_t pm) {
  Move m;
  m.fromRow = (pm & 63) >> 3;
  m.fromCol = (pm & 63) & 7;
  m.toRow = ((pm >> 6) & 63) >> 3;
  m.toCol = ((pm >> 6) & 63) & 7;
  m.promotion = (pm >> 12) & 0xF;
  m.isEP = (pm >> 16) & 1;
  m.isCastle = (pm >> 17) & 1;
  return m;
}

bool TT::probe(uint64_t k, int &score, int &depth, TTFlag &flag, Move &m) {
  if (!size)
    return false;
  TTEntry &e = table[k % size];
  uint64_t key_read = e.key.load();
  if (key_read != k)
    return false;
  uint64_t d = e.data.load();
  score = (int32_t)(uint32_t)d;
  depth = (d >> 32) & 0xFF;
  flag = (TTFlag)((d >> 40) & 3);
  m = unpackMove((d >> 42) & 0x3FFFF);
  return true;
}

void TT::store(uint64_t k, int score, int depth, TTFlag f, const Move &m) {
  if (!size)
    return;
  TTEntry &e = table[k % size];
  uint64_t existing_data = e.data.load();
  uint64_t existing_key = e.key.load();
  int existing_depth = (existing_data >> 32) & 0xFF;
  if (existing_key == k || depth >= existing_depth) {
    uint64_t d = (uint32_t)score | ((uint64_t)depth << 32) |
                 ((uint64_t)f << 40) | ((uint64_t)packMove(m) << 42);
    e.data.store(d);
    e.key.store(k);
  }
}