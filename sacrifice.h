#ifndef SACRIFICE_H
#define SACRIFICE_H

#include "board.h"
#include <string>
#include <vector>

// Materyal sayım yapısı
struct MaterialCount {
  int queens;
  int rooks;
  int lightPieces; // Bishop + Knight
  int pawns;

  MaterialCount() : queens(0), rooks(0), lightPieces(0), pawns(0) {}

  int total() const { return queens * 9 + rooks * 5 + lightPieces * 3 + pawns; }
};

// Sacrifice pattern koşulu
struct SacCondition {
  char piece;      // 'q', 'r', 'l', 'p'
  char comparison; // '=', '>', '<', '*'
  int value;       // Karşılaştırma değeri
  bool orMore;     // >= için
  bool orLess;     // <= için
};

// Sacrifice pattern
struct SacrificePattern {
  int priority; // Öncelik (derinlik gereksimi)
  std::vector<SacCondition> ourConditions;
  std::vector<SacCondition> theirConditions;
  int sacType; // 0=pawn, 1=piece, 2=exchange, 3=queen

  bool matches(const MaterialCount &us, const MaterialCount &them) const;
};

// Sacrifice türleri
enum SacrificeType {
  SAC_NONE = 0,
  SAC_PAWN = 1,
  SAC_PIECE = 2,
  SAC_EXCHANGE = 3,
  SAC_QUEEN = 4
};

// Sacrifice analiz sınıfı
class SacrificeAnalyzer {
public:
  std::vector<SacrificePattern> pawnSacPatterns;
  std::vector<SacrificePattern> queenSacPatterns;
  std::vector<SacrificePattern> pieceSacPatterns;

  bool loaded;

  SacrificeAnalyzer();

  // Pattern dosyalarını yükle
  bool loadPatterns(const std::string &sacrificeDir);

  // Tek dosya yükle
  bool loadPatternFile(const std::string &path, int sacType);

  // Pattern satırını parse et
  bool parsePatternLine(const std::string &line, SacrificePattern &pattern);

  // Pozisyonu analiz et
  int analyzeSacrifice(const Board &b, int side) const;

  // Materyal say
  MaterialCount countMaterial(const Board &b, int side) const;

  // Sacrifice bonusu hesapla
  int getSacrificeBonus(const Board &b, int side) const;

  // Saldırı potansiyeli var mı?
  bool hasAttackPotential(const Board &b, int side) const;
};

extern SacrificeAnalyzer sacrificeAnalyzer;

#endif
