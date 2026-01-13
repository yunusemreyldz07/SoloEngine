#include "evaluation.h"
#include "brain.h"
#include "params.h"
#include "sacrifice.h"
#include <cstdlib>

namespace {
constexpr int mirror(int sq) { return sq ^ 56; }

const int mgPawn[] = {
    0,   0,  0,   0,   0,   0,  0,  0,   98,  134, 61, 95,  68, 126, 34, -11,
    -6,  7,  26,  31,  65,  56, 25, -20, -14, 13,  6,  21,  23, 12,  17, -23,
    -27, -2, -5,  12,  17,  6,  10, -25, -26, -4,  -4, -10, 3,  3,   33, -12,
    -35, -1, -20, -23, -15, 24, 38, -22, 0,   0,   0,  0,   0,  0,   0,  0};
const int egPawn[] = {
    0,  0,   0,  0,  0,  0,  0,  0,  178, 173, 158, 134, 147, 132, 165, 187,
    94, 100, 85, 67, 56, 53, 82, 84, 32,  24,  13,  5,   -2,  4,   17,  17,
    13, 9,   -3, -7, -7, -8, 3,  -1, 4,   7,   -6,  1,   0,   -5,  -1,  -8,
    13, 8,   8,  10, 13, 0,  2,  -7, 0,   0,   0,   0,   0,   0,   0,   0};
const int mgKnight[] = {
    -167, -89, -34, -49, 61,   -97, -15, -107, -73, -41, 72,  36,  23,
    62,   7,   -17, -47, 60,   37,  65,  84,   129, 73,  44,  -9,  17,
    19,   53,  37,  69,  18,   22,  -13, 4,    16,  13,  28,  19,  21,
    -8,   -23, -9,  12,  10,   19,  17,  25,   -16, -29, -53, -12, -3,
    -1,   18,  -14, -19, -105, -21, -58, -33,  -17, -28, -19, -23};
const int egKnight[] = {-58, -38, -13, -28, -31, -27, -63, -99, -25, -8,  -25,
                        -2,  -9,  -25, -24, -52, -24, -20, 10,  9,   -1,  -9,
                        -19, -41, -17, 3,   22,  22,  22,  11,  8,   -18, -18,
                        -6,  16,  25,  16,  17,  4,   -18, -23, -3,  -1,  15,
                        10,  -3,  -20, -22, -42, -20, -10, -5,  -2,  -20, -23,
                        -44, -29, -51, -23, -15, -22, -18, -50, -64};
const int mgBishop[] = {
    -29, 4,  -82, -37, -25, -42, 7,  -8, -26, 16, -18, -13, 30,  59,  18,  -47,
    -16, 37, 43,  40,  35,  50,  37, -2, -4,  5,  19,  50,  37,  37,  7,   -2,
    -6,  13, 13,  26,  34,  12,  10, 4,  0,   15, 15,  15,  14,  27,  18,  10,
    4,   15, 16,  0,   7,   21,  33, 1,  -33, -3, -14, -21, -13, -12, -39, -21};
const int egBishop[] = {
    -14, -21, -11, -8, -7, -9, -17, -24, -8,  -4, 7,   -12, -3, -13, -4, -14,
    2,   -8,  0,   -1, -2, 6,  0,   4,   -3,  9,  12,  9,   14, 10,  3,  2,
    -6,  3,   13,  19, 7,  10, -3,  -9,  -12, -3, 8,   10,  13, 3,   -7, -15,
    -14, -18, -7,  -1, 4,  -9, -15, -27, -23, -9, -23, -5,  -9, -16, -5, -17};
const int mgRook[] = {
    15,  -10, -5, 0,  0,  -5, -10, 15,  10,  10,  10, 15, 15, 10, 10,  10,
    -10, -5,  0,  5,  5,  0,  -5,  -10, -10, -5,  0,  5,  5,  0,  -5,  -10,
    -10, -5,  0,  5,  5,  0,  -5,  -10, -10, -5,  0,  5,  5,  0,  -5,  -10,
    10,  10,  10, 15, 15, 10, 10,  10,  15,  -10, -5, 0,  0,  -5, -10, 15};
const int egRook[] = {13,  10, 18,  15, 12, 12, 8,   5,  11,  13,  13, 11, -3,
                      3,   8,  3,   7,  7,  7,  5,   4,  -3,  -5,  -3, 4,  3,
                      13,  1,  2,   1,  -1, 2,  3,   5,  8,   4,   -5, -6, -8,
                      -11, -4, 0,   -5, -1, -7, -12, -8, -16, -6,  -6, 0,  2,
                      -9,  -9, -11, -3, -9, 2,  3,   -1, -5,  -13, 4,  -20};
const int mgQueen[] = {
    -28, 0,   29, 12,  59, 44, 43, 45, -24, -39, -5,  1,   -16, 57,  28,  54,
    -13, -17, 7,  8,   29, 56, 47, 57, -27, -27, -16, -16, -1,  17,  -2,  1,
    -9,  -26, -9, -10, -2, -4, 3,  -3, -14, 2,   -11, -2,  -5,  2,   14,  5,
    -35, -8,  11, 2,   8,  15, -3, 1,  -1,  -18, -9,  10,  -15, -25, -31, -50};
const int egQueen[] = {-9,  22,  22,  27,  27,  19,  10,  20,  -17, 20,  32,
                       41,  58,  25,  30,  0,   -20, 6,   9,   49,  47,  35,
                       19,  9,   3,   22,  24,  45,  57,  40,  57,  36,  -18,
                       28,  19,  47,  31,  34,  39,  23,  -16, -27, 15,  6,
                       9,   17,  10,  5,   -22, -23, -30, -16, -16, -23, -36,
                       -32, -33, -28, -22, -43, -5,  -32, -20, -41};
const int mgKing[] = {-65, 23,  16,  -15, -56, -34, 2,   13,  29,  -1,  -20,
                      -7,  -8,  -4,  -38, -29, -9,  24,  2,   -16, -20, 6,
                      22,  -22, -17, -20, -12, -27, -30, -25, -14, -36, -49,
                      -1,  -27, -39, -46, -44, -33, -51, -14, -14, -22, -46,
                      -44, -30, -15, -27, 1,   7,   -8,  -64, -43, -16, 9,
                      8,   -15, 36,  12,  -54, 8,   -28, 24,  14};
const int egKing[] = {-74, -35, -18, -18, -11, 15,  4,   -17, -12, 17, 14,
                      17,  17,  38,  23,  11,  10,  17,  23,  15,  20, 45,
                      44,  13,  -8,  22,  24,  27,  26,  33,  26,  3,  -18,
                      -4,  21,  24,  27,  23,  9,   -11, -19, -3,  11, 21,
                      23,  16,  7,   -9,  -27, -11, 4,   13,  14,  4,  -5,
                      -17, -53, -34, -21, -11, -28, -14, -24, -43};

const int *mgTables[] = {mgPawn, mgKnight, mgBishop, mgRook, mgQueen, mgKing};
const int *egTables[] = {egPawn, egKnight, egBishop, egRook, egQueen, egKing};

int mgTable[12][64], egTable[12][64];
bool tableInit = false;

void initTables() {
  const int pieceVals[] = {params.pawnValue,   params.knightValue,
                           params.bishopValue, params.rookValue,
                           params.queenValue,  0};

  for (int p = 0; p < 6; p++) {
    for (int sq = 0; sq < 64; sq++) {
      int msq = mirror(sq);
      mgTable[p * 2][sq] = pieceVals[p] + mgTables[p][sq];
      egTable[p * 2][sq] = pieceVals[p] + egTables[p][sq];
      mgTable[p * 2 + 1][sq] = pieceVals[p] + mgTables[p][msq];
      egTable[p * 2 + 1][sq] = pieceVals[p] + egTables[p][msq];
    }
  }
  tableInit = true;
}

int evaluatePawns(const Board &b, int side) {
  int score = 0;
  int pawnPiece = (side == 0) ? 1 : -1;
  bool hasPawn[8] = {false};
  int pawnRanks[8][8];
  int pawnCount[8] = {0};

  for (int file = 0; file < 8; file++) {
    for (int rank = 0; rank < 8; rank++) {
      int sq = rank * 8 + file;
      if (b.squares[sq] == pawnPiece) {
        hasPawn[file] = true;
        pawnRanks[file][pawnCount[file]++] = rank;
      }
    }
  }

  for (int file = 0; file < 8; file++) {
    if (!hasPawn[file])
      continue;

    bool hasLeft = (file > 0) && hasPawn[file - 1];
    bool hasRight = (file < 7) && hasPawn[file + 1];

    if (!hasLeft && !hasRight) {
      score -= 15;
    }

    if (pawnCount[file] > 1) {
      score -= 10 * (pawnCount[file] - 1);
    }

    for (int i = 0; i < pawnCount[file]; i++) {
      int rank = pawnRanks[file][i];
      int relRank = (side == 0) ? rank : (7 - rank);

      if (relRank >= 4) {
        bool passed = true;
        for (int checkFile = file - 1; checkFile <= file + 1; checkFile++) {
          if (checkFile < 0 || checkFile > 7)
            continue;
          for (int checkRank = rank + 1; checkRank < 8; checkRank++) {
            int checkSq = checkRank * 8 + checkFile;
            if (b.squares[checkSq] == -pawnPiece) {
              passed = false;
              break;
            }
          }
          if (!passed)
            break;
        }

        if (passed) {
          int bonus = 20 + (relRank - 4) * 20;
          score += bonus;
        }
      }
    }
  }

  return score;
}

int evaluateKingSafety(const Board &b, int kingSq, int side) {
  int safety = 0;
  int kingFile = kingSq % 8;
  int kingRank = kingSq / 8;
  int pawnPiece = (side == 0) ? 1 : -1;
  int pawnRank = (side == 0) ? 1 : 6;

  if (kingFile >= 5) {
    int f2 = pawnRank * 8 + 5;
    int g2 = pawnRank * 8 + 6;
    int h2 = pawnRank * 8 + 7;
    if (b.squares[f2] == pawnPiece)
      safety += 10;
    if (b.squares[g2] == pawnPiece)
      safety += 15;
    if (b.squares[h2] == pawnPiece)
      safety += 10;
  } else if (kingFile <= 2) {
    int a2 = pawnRank * 8 + 0;
    int b2 = pawnRank * 8 + 1;
    int c2 = pawnRank * 8 + 2;
    if (b.squares[a2] == pawnPiece)
      safety += 10;
    if (b.squares[b2] == pawnPiece)
      safety += 15;
    if (b.squares[c2] == pawnPiece)
      safety += 10;
  }

  for (int file = std::max(0, kingFile - 1); file <= std::min(7, kingFile + 1);
       file++) {
    bool hasFriendlyPawn = false;
    bool hasEnemyPawn = false;
    for (int rank = 0; rank < 8; rank++) {
      int sq = rank * 8 + file;
      if (b.squares[sq] == pawnPiece)
        hasFriendlyPawn = true;
      if (b.squares[sq] == -pawnPiece)
        hasEnemyPawn = true;
    }
    if (!hasFriendlyPawn && !hasEnemyPawn)
      safety -= 20;
    else if (!hasFriendlyPawn)
      safety -= 10;
  }

  return safety;
}

int evaluateMobility(const Board &b, int side) {
  int mobility = 0;

  for (int sq = 0; sq < 64; sq++) {
    int piece = b.squares[sq];
    if (!piece)
      continue;
    int pieceSide = piece > 0 ? 0 : 1;
    if (pieceSide != side)
      continue;

    int pt = std::abs(piece) - 1;
    if (pt == 0 || pt == 5)
      continue;

    uint64_t attacks = 0;
    if (pt == 1) {
      attacks = knight_attacks(sq);
    } else if (pt == 2) {
      uint64_t occ = 0;
      for (int i = 0; i < 64; i++) {
        if (b.squares[i])
          occ |= (1ULL << i);
      }
      attacks = bishop_attacks(sq, occ);
    } else if (pt == 3) {
      uint64_t occ = 0;
      for (int i = 0; i < 64; i++) {
        if (b.squares[i])
          occ |= (1ULL << i);
      }
      attacks = rook_attacks(sq, occ);
    } else if (pt == 4) {
      uint64_t occ = 0;
      for (int i = 0; i < 64; i++) {
        if (b.squares[i])
          occ |= (1ULL << i);
      }
      attacks = queen_attacks(sq, occ);
    }

    int count = __builtin_popcountll(attacks);

    if (pt == 1)
      mobility += count * 3;
    else if (pt == 2)
      mobility += count * 2;
    else
      mobility += count;
  }

  return mobility;
}

int evaluateBishopPair(const Board &b, int side) {
  int bishopCount = 0;
  int bishopPiece = (side == 0) ? 3 : -3;
  for (int sq = 0; sq < 64; sq++) {
    if (b.squares[sq] == bishopPiece)
      bishopCount++;
  }
  return (bishopCount >= 2) ? 30 : 0;
}

int evaluateRooksOnFiles(const Board &b, int side) {
  int score = 0;
  int rookPiece = (side == 0) ? 4 : -4;
  int friendlyPawn = (side == 0) ? 1 : -1;
  int enemyPawn = -friendlyPawn;

  for (int sq = 0; sq < 64; sq++) {
    if (b.squares[sq] != rookPiece)
      continue;
    int file = sq % 8;
    bool hasFriendlyPawn = false;
    bool hasEnemyPawn = false;

    for (int rank = 0; rank < 8; rank++) {
      int checkSq = rank * 8 + file;
      if (b.squares[checkSq] == friendlyPawn)
        hasFriendlyPawn = true;
      if (b.squares[checkSq] == enemyPawn)
        hasEnemyPawn = true;
    }

    if (!hasFriendlyPawn && !hasEnemyPawn)
      score += 25;
    else if (!hasFriendlyPawn)
      score += 15;
  }
  return score;
}

int evaluateKnightOutposts(const Board &b, int side) {
  int score = 0;
  int knightPiece = (side == 0) ? 2 : -2;
  int friendlyPawn = (side == 0) ? 1 : -1;
  int enemyPawn = -friendlyPawn;

  for (int sq = 0; sq < 64; sq++) {
    if (b.squares[sq] != knightPiece)
      continue;
    int file = sq % 8;
    int rank = sq / 8;
    int relRank = (side == 0) ? (7 - rank) : rank;

    if (relRank < 3 || relRank > 5)
      continue;
    if (file == 0 || file == 7)
      continue;

    bool canBeAttacked = false;
    int startRank = (side == 0) ? rank + 1 : 0;
    int endRank = (side == 0) ? 8 : rank;

    for (int r = startRank; r < endRank; r++) {
      if (file > 0 && b.squares[r * 8 + file - 1] == enemyPawn)
        canBeAttacked = true;
      if (file < 7 && b.squares[r * 8 + file + 1] == enemyPawn)
        canBeAttacked = true;
    }

    if (!canBeAttacked) {
      score += 20;
      if (file > 0 && b.squares[sq - 9 + (side == 0 ? 0 : 16)] == friendlyPawn)
        score += 10;
      if (file < 7 && b.squares[sq - 7 + (side == 0 ? 0 : 16)] == friendlyPawn)
        score += 10;
    }
  }
  return score;
}

int evaluateConnectedRooks(const Board &b, int side) {
  int rookPiece = (side == 0) ? 4 : -4;
  int rooks[2] = {-1, -1};
  int rookCount = 0;

  for (int sq = 0; sq < 64 && rookCount < 2; sq++) {
    if (b.squares[sq] == rookPiece)
      rooks[rookCount++] = sq;
  }

  if (rookCount < 2)
    return 0;

  int r1 = rooks[0], r2 = rooks[1];
  int file1 = r1 % 8, rank1 = r1 / 8;
  int file2 = r2 % 8, rank2 = r2 / 8;

  if (rank1 == rank2) {
    bool clear = true;
    for (int f = std::min(file1, file2) + 1; f < std::max(file1, file2); f++) {
      if (b.squares[rank1 * 8 + f]) {
        clear = false;
        break;
      }
    }
    if (clear)
      return 15;
  }

  if (file1 == file2) {
    bool clear = true;
    for (int r = std::min(rank1, rank2) + 1; r < std::max(rank1, rank2); r++) {
      if (b.squares[r * 8 + file1]) {
        clear = false;
        break;
      }
    }
    if (clear)
      return 15;
  }

  return 0;
}

int evaluateRookOn7th(const Board &b, int side) {
  int score = 0;
  int rookPiece = (side == 0) ? 4 : -4;
  int targetRank = (side == 0) ? 1 : 6;

  for (int sq = 0; sq < 64; sq++) {
    if (b.squares[sq] == rookPiece && sq / 8 == targetRank) {
      score += 30;
    }
  }
  return score;
}

int evaluateSpace(const Board &b, int side) {
  int score = 0;
  int pawnPiece = (side == 0) ? 1 : -1;

  for (int sq = 0; sq < 64; sq++) {
    if (b.squares[sq] == pawnPiece) {
      int rank = sq / 8;
      int relRank = (side == 0) ? (7 - rank) : rank;
      if (relRank >= 3 && relRank <= 5) {
        int file = sq % 8;
        if (file >= 2 && file <= 5)
          score += 3;
      }
    }
  }
  return score;
}

int evaluateQueenTropism(const Board &b, int side) {
  int queenPiece = (side == 0) ? 5 : -5;
  int enemyKingSq = (side == 0) ? b.bKingSq : b.wKingSq;
  int kingFile = enemyKingSq % 8;
  int kingRank = enemyKingSq / 8;

  for (int sq = 0; sq < 64; sq++) {
    if (b.squares[sq] == queenPiece) {
      int qFile = sq % 8;
      int qRank = sq / 8;
      int dist =
          std::max(std::abs(qFile - kingFile), std::abs(qRank - kingRank));
      return (7 - dist) * 3;
    }
  }
  return 0;
}

int evaluateThreats(const Board &b, int side) {
  int score = 0;
  int pawnPiece = (side == 0) ? 1 : -1;

  for (int sq = 0; sq < 64; sq++) {
    int piece = b.squares[sq];
    if (!piece)
      continue;
    int pieceSide = piece > 0 ? 0 : 1;
    if (pieceSide != side)
      continue;

    int pt = std::abs(piece) - 1;
    if (pt != 0)
      continue;

    int dir = (side == 0) ? -1 : 1;
    int file = sq % 8;

    if (file > 0) {
      int attackSq = sq + dir * 8 - 1;
      if (attackSq >= 0 && attackSq < 64) {
        int target = b.squares[attackSq];
        if (target && (target > 0 ? 0 : 1) != side) {
          int targetPt = std::abs(target) - 1;
          if (targetPt >= 1 && targetPt <= 4)
            score += 15 + targetPt * 5;
        }
      }
    }

    if (file < 7) {
      int attackSq = sq + dir * 8 + 1;
      if (attackSq >= 0 && attackSq < 64) {
        int target = b.squares[attackSq];
        if (target && (target > 0 ? 0 : 1) != side) {
          int targetPt = std::abs(target) - 1;
          if (targetPt >= 1 && targetPt <= 4)
            score += 15 + targetPt * 5;
        }
      }
    }
  }
  return score;
}

int evaluateKingProximity(const Board &b, int side) {
  int score = 0;
  int kingSq = (side == 0) ? b.wKingSq : b.bKingSq;
  int kingFile = kingSq % 8;
  int kingRank = kingSq / 8;
  int pawnPiece = (side == 0) ? 1 : -1;

  for (int sq = 0; sq < 64; sq++) {
    if (b.squares[sq] == pawnPiece) {
      int pFile = sq % 8;
      int pRank = sq / 8;
      int dist =
          std::max(std::abs(pFile - kingFile), std::abs(pRank - kingRank));
      int relRank = (side == 0) ? (7 - pRank) : pRank;
      if (relRank >= 4) {
        score += (7 - dist) * 3;
      }
    }
  }
  return score;
}

int evaluateBadBishop(const Board &b, int side) {
  int penalty = 0;
  int bishopPiece = (side == 0) ? 3 : -3;
  int pawnPiece = (side == 0) ? 1 : -1;

  for (int sq = 0; sq < 64; sq++) {
    if (b.squares[sq] == bishopPiece) {
      bool isLightSquare = ((sq / 8) + (sq % 8)) % 2 == 0;
      int blockedPawns = 0;

      for (int psq = 0; psq < 64; psq++) {
        if (b.squares[psq] == pawnPiece) {
          bool pawnLight = ((psq / 8) + (psq % 8)) % 2 == 0;
          if (pawnLight == isLightSquare)
            blockedPawns++;
        }
      }
      penalty += blockedPawns * 5;
    }
  }
  return -penalty;
}

int evaluatePawnStorm(const Board &b, int side) {
  int score = 0;
  int pawnPiece = (side == 0) ? 1 : -1;
  int enemyKingSq = (side == 0) ? b.bKingSq : b.wKingSq;
  int enemyKingFile = enemyKingSq % 8;

  for (int sq = 0; sq < 64; sq++) {
    if (b.squares[sq] == pawnPiece) {
      int pFile = sq % 8;
      int pRank = sq / 8;
      int relRank = (side == 0) ? (7 - pRank) : pRank;

      if (std::abs(pFile - enemyKingFile) <= 1 && relRank >= 4) {
        score += (relRank - 3) * 8;
      }
    }
  }
  return score;
}

// ==================== AGRESIF EVALUATION FONKSIYONLARI ====================

// King Attack Units - Stockfish tarzı saldırı birim hesaplaması
int evaluateKingAttackUnits(const Board &b, int side) {
  int units = 0;
  int enemyKingSq = (side == 0) ? b.bKingSq : b.wKingSq;
  int kingFile = enemyKingSq % 8;
  int kingRank = enemyKingSq / 8;

  // Kral çevresindeki kareler
  uint64_t kingZone = king_attacks(enemyKingSq);
  // Genişletilmiş kral bölgesi
  for (int i = 0; i < 64; i++) {
    if (kingZone & (1ULL << i)) {
      kingZone |= king_attacks(i);
    }
  }

  int attackerCount = 0;
  int attackWeight = 0;

  for (int sq = 0; sq < 64; sq++) {
    int piece = b.squares[sq];
    if (!piece)
      continue;
    int pieceSide = piece > 0 ? 0 : 1;
    if (pieceSide != side)
      continue;

    int pt = std::abs(piece) - 1;
    uint64_t attacks = 0;
    uint64_t occ = 0;

    // Occupancy hesapla
    for (int i = 0; i < 64; i++) {
      if (b.squares[i])
        occ |= (1ULL << i);
    }

    switch (pt) {
    case 1: // Knight
      attacks = knight_attacks(sq);
      break;
    case 2: // Bishop
      attacks = bishop_attacks(sq, occ);
      break;
    case 3: // Rook
      attacks = rook_attacks(sq, occ);
      break;
    case 4: // Queen
      attacks = queen_attacks(sq, occ);
      break;
    default:
      continue;
    }

    // Kral bölgesine saldırı var mı?
    uint64_t kingAttacks = attacks & kingZone;
    if (kingAttacks) {
      attackerCount++;
      int popCnt = __builtin_popcountll(kingAttacks);

      // Taş tipine göre ağırlık
      switch (pt) {
      case 1:
        attackWeight += popCnt * 2;
        break; // Knight
      case 2:
        attackWeight += popCnt * 2;
        break; // Bishop
      case 3:
        attackWeight += popCnt * 3;
        break; // Rook
      case 4:
        attackWeight += popCnt * 5;
        break; // Queen
      }
    }
  }

  // Saldırgan sayısına göre çarpan
  static const int attackerMultiplier[] = {0, 0, 50, 75, 88, 94, 97, 99, 100};
  int multiplier = attackerCount < 9 ? attackerMultiplier[attackerCount] : 100;

  units = attackWeight * multiplier / 100;

  return units;
}

// Saldırı Potansiyeli - Açık hatlar, fil diyagonalleri
int evaluateAttackPotential(const Board &b, int side) {
  int potential = 0;
  int enemyKingSq = (side == 0) ? b.bKingSq : b.wKingSq;
  int enemyKingFile = enemyKingSq % 8;
  int enemyKingRank = enemyKingSq / 8;

  // Kral tarafındaki açık hatlar
  int pawnPiece = (side == 0) ? 1 : -1;
  int enemyPawn = -pawnPiece;

  for (int file = std::max(0, enemyKingFile - 1);
       file <= std::min(7, enemyKingFile + 1); file++) {
    bool hasFriendlyPawn = false;
    bool hasEnemyPawn = false;

    for (int rank = 0; rank < 8; rank++) {
      int sq = rank * 8 + file;
      if (b.squares[sq] == pawnPiece)
        hasFriendlyPawn = true;
      if (b.squares[sq] == enemyPawn)
        hasEnemyPawn = true;
    }

    if (!hasFriendlyPawn && !hasEnemyPawn) {
      potential += 20; // Açık hat bonusu
    } else if (!hasEnemyPawn) {
      potential += 12; // Yarı açık hat bonusu
    }
  }

  // Vezir + Kale kombinasyonu
  int queenPiece = (side == 0) ? 5 : -5;
  int rookPiece = (side == 0) ? 4 : -4;
  bool hasQueen = false;
  int rookCount = 0;

  for (int sq = 0; sq < 64; sq++) {
    if (b.squares[sq] == queenPiece)
      hasQueen = true;
    if (b.squares[sq] == rookPiece)
      rookCount++;
  }

  if (hasQueen && rookCount >= 1)
    potential += 25;
  if (hasQueen && rookCount >= 2)
    potential += 15;

  return potential;
}

// Materyal Dengesizliği - Eşit olmayan değişimler için bonus
int evaluateMaterialImbalance(const Board &b, int side) {
  int imbalance = 0;

  // Materyal sayımı
  int wBishops = 0, wKnights = 0, wRooks = 0, wQueens = 0;
  int bBishops = 0, bKnights = 0, bRooks = 0, bQueens = 0;

  for (int sq = 0; sq < 64; sq++) {
    int piece = b.squares[sq];
    if (!piece)
      continue;

    int pt = std::abs(piece);
    bool isWhite = piece > 0;

    switch (pt) {
    case 2:
      if (isWhite)
        wKnights++;
      else
        bKnights++;
      break;
    case 3:
      if (isWhite)
        wBishops++;
      else
        bBishops++;
      break;
    case 4:
      if (isWhite)
        wRooks++;
      else
        bRooks++;
      break;
    case 5:
      if (isWhite)
        wQueens++;
      else
        bQueens++;
      break;
    }
  }

  if (side == 0) {
    // Rook vs 2 minor pieces
    if (wRooks > bRooks && (wBishops + wKnights) < (bBishops + bKnights))
      imbalance += 15;
    // Bishop pair vs knight pair
    if (wBishops >= 2 && bBishops < 2)
      imbalance += 10;
  } else {
    if (bRooks > wRooks && (bBishops + bKnights) < (wBishops + wKnights))
      imbalance += 15;
    if (bBishops >= 2 && wBishops < 2)
      imbalance += 10;
  }

  return imbalance;
}

// Tempo Bonusu - İnisiyatif ve gelişim
int evaluateTempoBonus(const Board &b, int side) {
  int tempo = 0;

  // Merkez kontrolü
  int centerSquares[] = {27, 28, 35, 36}; // d4, e4, d5, e5
  int pawnPiece = (side == 0) ? 1 : -1;
  int knightPiece = (side == 0) ? 2 : -2;
  int bishopPiece = (side == 0) ? 3 : -3;

  for (int i = 0; i < 4; i++) {
    int sq = centerSquares[i];
    if (b.squares[sq] == pawnPiece)
      tempo += 8;
    if (b.squares[sq] == knightPiece)
      tempo += 10;
    if (b.squares[sq] == bishopPiece)
      tempo += 8;
  }

  // Genişletilmiş merkez
  int extendedCenter[] = {18, 19, 20, 21, 26, 29, 34, 37, 42, 43, 44, 45};
  for (int i = 0; i < 12; i++) {
    int sq = extendedCenter[i];
    if (b.squares[sq] == pawnPiece)
      tempo += 3;
    if (b.squares[sq] == knightPiece)
      tempo += 5;
  }

  return tempo;
}

// Hanging Pieces (Asılı Taşlar) - Savunmasız taşları tespit et
int evaluateHangingPieces(const Board &b, int side) {
  int penalty = 0;
  int enemySide = 1 - side;

  uint64_t occ = 0;
  for (int i = 0; i < 64; i++) {
    if (b.squares[i])
      occ |= (1ULL << i);
  }

  // Düşman saldırılarını hesapla
  uint64_t enemyAttacks = 0;
  for (int sq = 0; sq < 64; sq++) {
    int piece = b.squares[sq];
    if (!piece)
      continue;
    int pieceSide = piece > 0 ? 0 : 1;
    if (pieceSide != enemySide)
      continue;

    int pt = std::abs(piece) - 1;
    switch (pt) {
    case 0: { // Pawn
      int dir = (enemySide == 0) ? -1 : 1;
      int file = sq % 8;
      if (file > 0)
        enemyAttacks |= (1ULL << (sq + dir * 8 - 1));
      if (file < 7)
        enemyAttacks |= (1ULL << (sq + dir * 8 + 1));
      break;
    }
    case 1:
      enemyAttacks |= knight_attacks(sq);
      break;
    case 2:
      enemyAttacks |= bishop_attacks(sq, occ);
      break;
    case 3:
      enemyAttacks |= rook_attacks(sq, occ);
      break;
    case 4:
      enemyAttacks |= queen_attacks(sq, occ);
      break;
    case 5:
      enemyAttacks |= king_attacks(sq);
      break;
    }
  }

  // Bizim taşlarımızın savunmasını kontrol et
  uint64_t ourDefense = 0;
  for (int sq = 0; sq < 64; sq++) {
    int piece = b.squares[sq];
    if (!piece)
      continue;
    int pieceSide = piece > 0 ? 0 : 1;
    if (pieceSide != side)
      continue;

    int pt = std::abs(piece) - 1;
    switch (pt) {
    case 0: {
      int dir = (side == 0) ? -1 : 1;
      int file = sq % 8;
      if (file > 0 && sq + dir * 8 - 1 >= 0 && sq + dir * 8 - 1 < 64)
        ourDefense |= (1ULL << (sq + dir * 8 - 1));
      if (file < 7 && sq + dir * 8 + 1 >= 0 && sq + dir * 8 + 1 < 64)
        ourDefense |= (1ULL << (sq + dir * 8 + 1));
      break;
    }
    case 1:
      ourDefense |= knight_attacks(sq);
      break;
    case 2:
      ourDefense |= bishop_attacks(sq, occ);
      break;
    case 3:
      ourDefense |= rook_attacks(sq, occ);
      break;
    case 4:
      ourDefense |= queen_attacks(sq, occ);
      break;
    case 5:
      ourDefense |= king_attacks(sq);
      break;
    }
  }

  // Savunmasız ve saldırı altındaki taşları bul
  for (int sq = 0; sq < 64; sq++) {
    int piece = b.squares[sq];
    if (!piece)
      continue;
    int pieceSide = piece > 0 ? 0 : 1;
    if (pieceSide != side)
      continue;

    int pt = std::abs(piece);
    if (pt == 6)
      continue; // Kral hariç

    bool attacked = (enemyAttacks & (1ULL << sq)) != 0;
    bool defended = (ourDefense & (1ULL << sq)) != 0;

    if (attacked && !defended) {
      // Savunmasız taş cezası
      static const int pieceValues[] = {0, 100, 320, 330, 500, 900, 0};
      penalty += pieceValues[pt] / 4;
    }
  }

  return -penalty; // Negatif (ceza)
}

// Taş Yoğunluğu - Düşman kralına doğru taş konsantrasyonu
int evaluatePieceConcentration(const Board &b, int side) {
  int concentration = 0;
  int enemyKingSq = (side == 0) ? b.bKingSq : b.wKingSq;
  int enemyKingFile = enemyKingSq % 8;
  int enemyKingRank = enemyKingSq / 8;

  for (int sq = 0; sq < 64; sq++) {
    int piece = b.squares[sq];
    if (!piece)
      continue;
    int pieceSide = piece > 0 ? 0 : 1;
    if (pieceSide != side)
      continue;

    int pt = std::abs(piece);
    if (pt == 1 || pt == 6)
      continue; // Piyon ve kral hariç

    int file = sq % 8;
    int rank = sq / 8;
    int dist = std::max(std::abs(file - enemyKingFile),
                        std::abs(rank - enemyKingRank));

    // Mesafe ne kadar yakınsa o kadar bonus
    int bonus = (7 - dist) * 2;

    // Taş tipine göre ağırlıklandır
    if (pt == 4 || pt == 5) // Rook veya Queen
      bonus = bonus * 3 / 2;

    concentration += bonus;
  }

  return concentration;
}

// Sacrifice Değerlendirmesi - sacrifice.cpp'den
int evaluateSacrificeBonus(const Board &b, int side) {
  return sacrificeAnalyzer.getSacrificeBonus(b, side);
}
} // namespace

void reinitEvaluation() { tableInit = false; }

int evaluate(const Board &b) {
  if (useBrain) {
    return brain.evaluate(b);
  }
  if (!tableInit)
    initTables();
  int mg[2] = {0, 0}, eg[2] = {0, 0}, phase = 0;
  for (int sq = 0; sq < 64; sq++) {
    int p = b.squares[sq];
    if (!p)
      continue;
    int side = p > 0 ? 0 : 1;
    int pt = std::abs(p) - 1;
    int idx = pt * 2 + side;
    mg[side] += mgTable[idx][sq];
    eg[side] += egTable[idx][sq];
    phase += (pt == 0   ? 0
              : pt == 1 ? 1
              : pt == 2 ? 1
              : pt == 3 ? 2
              : pt == 4 ? 4
                        : 0);
  }

  int pawnScore = evaluatePawns(b, 0) - evaluatePawns(b, 1);

  int mgScore = mg[0] - mg[1] + pawnScore;
  int egScore = eg[0] - eg[1] + pawnScore * 2;

  if (phase > 12) {
    int kingSafety = evaluateKingSafety(b, b.wKingSq, 0) -
                     evaluateKingSafety(b, b.bKingSq, 1);
    mgScore += kingSafety;
  }

  int mobility = evaluateMobility(b, 0) - evaluateMobility(b, 1);
  mgScore += mobility;
  egScore += mobility / 2;

  int bishopPair = evaluateBishopPair(b, 0) - evaluateBishopPair(b, 1);
  mgScore += bishopPair;
  egScore += bishopPair;

  int rookFiles = evaluateRooksOnFiles(b, 0) - evaluateRooksOnFiles(b, 1);
  mgScore += rookFiles;
  egScore += rookFiles;

  int outposts = evaluateKnightOutposts(b, 0) - evaluateKnightOutposts(b, 1);
  mgScore += outposts;

  int connectedRooks =
      evaluateConnectedRooks(b, 0) - evaluateConnectedRooks(b, 1);
  mgScore += connectedRooks;
  egScore += connectedRooks;

  int rookOn7th = evaluateRookOn7th(b, 0) - evaluateRookOn7th(b, 1);
  mgScore += rookOn7th;
  egScore += rookOn7th * 2;

  int space = evaluateSpace(b, 0) - evaluateSpace(b, 1);
  mgScore += space;

  int queenTropism = evaluateQueenTropism(b, 0) - evaluateQueenTropism(b, 1);
  if (phase > 12)
    mgScore += queenTropism;

  int threats = evaluateThreats(b, 0) - evaluateThreats(b, 1);
  mgScore += threats;
  egScore += threats;

  if (phase < 12) {
    int kingProx = evaluateKingProximity(b, 0) - evaluateKingProximity(b, 1);
    egScore += kingProx;
  }

  int badBishop = evaluateBadBishop(b, 0) - evaluateBadBishop(b, 1);
  mgScore += badBishop;
  egScore += badBishop;

  if (phase > 12) {
    int pawnStorm = evaluatePawnStorm(b, 0) - evaluatePawnStorm(b, 1);
    mgScore += pawnStorm;
  }

  // ==================== AGRESIF DEĞERLENDIRME ====================
  // Agresiflik çarpanı (100 = normal, 150 = agresif, 200 = ultra agresif)
  int aggMult = params.aggressiveness;

  // King Attack Units - Düşman kralına saldırı
  if (phase > 8) {
    int kingAttack =
        evaluateKingAttackUnits(b, 0) - evaluateKingAttackUnits(b, 1);
    mgScore += kingAttack * aggMult / 50; // Agresiflik ile ölçekle
  }

  // Attack Potential - Saldırı potansiyeli
  int attackPot = evaluateAttackPotential(b, 0) - evaluateAttackPotential(b, 1);
  mgScore += attackPot * aggMult / 100;

  // Material Imbalance - Materyal dengesizliği
  int imbalance =
      evaluateMaterialImbalance(b, 0) - evaluateMaterialImbalance(b, 1);
  mgScore += imbalance;
  egScore += imbalance;

  // Tempo Bonus - İnisiyatif
  int tempo = evaluateTempoBonus(b, 0) - evaluateTempoBonus(b, 1);
  mgScore += tempo * aggMult / 100;

  // Hanging Pieces - Savunmasız taşlar
  int hanging = evaluateHangingPieces(b, 0) - evaluateHangingPieces(b, 1);
  mgScore += hanging;
  egScore += hanging;

  // Piece Concentration - Taş yoğunluğu düşman kralına
  if (phase > 10) {
    int concentration =
        evaluatePieceConcentration(b, 0) - evaluatePieceConcentration(b, 1);
    mgScore += concentration * aggMult / 80;
  }

  // Sacrifice Bonus - Fedakarlık değerlendirmesi
  int sacBonus = evaluateSacrificeBonus(b, 0) - evaluateSacrificeBonus(b, 1);
  mgScore += sacBonus * aggMult / 100;
  egScore += sacBonus * aggMult / 150;

  // ==================== FİNAL SKOR ====================
  int p = phase * 256 / 24;
  int score = (mgScore * p + egScore * (256 - p)) / 256;
  return b.whiteTurn ? score : -score;
}
