#ifndef SACRIFICE_H
#define SACRIFICE_H

#include "board.h"
#include <string>
#include <vector>

struct MaterialCount {
  int queens;
  int rooks;
  int lightPieces;
  int pawns;

  MaterialCount() : queens(0), rooks(0), lightPieces(0), pawns(0) {}

  int total() const { return queens * 9 + rooks * 5 + lightPieces * 3 + pawns; }
};

struct SacCondition {
  char piece;
  char comparison;
  int value;
  bool orMore;
  bool orLess;
};

struct SacrificePattern {
  int priority;
  std::vector<SacCondition> ourConditions;
  std::vector<SacCondition> theirConditions;
  int sacType;

  bool matches(const MaterialCount &us, const MaterialCount &them) const;
};

enum SacrificeType {
  SAC_NONE = 0,
  SAC_PAWN = 1,
  SAC_PIECE = 2,
  SAC_EXCHANGE = 3,
  SAC_QUEEN = 4
};

class SacrificeAnalyzer {
public:
  std::vector<SacrificePattern> pawnSacPatterns;
  std::vector<SacrificePattern> queenSacPatterns;
  std::vector<SacrificePattern> pieceSacPatterns;

  bool loaded;

  SacrificeAnalyzer();

  bool loadPatterns(const std::string &sacrificeDir);
  bool loadPatternFile(const std::string &path, int sacType);

  bool parsePatternLine(const std::string &line, SacrificePattern &pattern);
  int analyzeSacrifice(const Board &b, int side) const;

  MaterialCount countMaterial(const Board &b, int side) const;

  int getSacrificeBonus(const Board &b, int side) const;

  bool hasAttackPotential(const Board &b, int side) const;
};

extern SacrificeAnalyzer sacrificeAnalyzer;

#endif
