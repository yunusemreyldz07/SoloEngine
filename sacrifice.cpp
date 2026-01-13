#include "sacrifice.h"
#include "bitboard.h"
#include <algorithm>
#include <cstdlib>
#include <dirent.h>
#include <fstream>
#include <iostream>
#include <sstream>

SacrificeAnalyzer sacrificeAnalyzer;

SacrificeAnalyzer::SacrificeAnalyzer() : loaded(false) {}

bool SacCondition_matches(const SacCondition &cond, int value) {
  switch (cond.comparison) {
  case '*':
    return true; // Herhangi bir değer
  case '=':
    return value == cond.value;
  case '>':
    if (cond.orMore)
      return value >= cond.value;
    return value > cond.value;
  case '<':
    if (cond.orLess)
      return value <= cond.value;
    return value < cond.value;
  default:
    return true;
  }
}

bool SacrificePattern::matches(const MaterialCount &us,
                               const MaterialCount &them) const {
  for (const auto &cond : ourConditions) {
    int val = 0;
    switch (cond.piece) {
    case 'q':
      val = us.queens;
      break;
    case 'r':
      val = us.rooks;
      break;
    case 'l':
      val = us.lightPieces;
      break;
    case 'p':
      val = us.pawns;
      break;
    }
    if (!SacCondition_matches(cond, val))
      return false;
  }

  for (const auto &cond : theirConditions) {
    int val = 0;
    switch (cond.piece) {
    case 'q':
      val = them.queens;
      break;
    case 'r':
      val = them.rooks;
      break;
    case 'l':
      val = them.lightPieces;
      break;
    case 'p':
      val = them.pawns;
      break;
    }
    if (!SacCondition_matches(cond, val))
      return false;
  }

  return true;
}

bool SacrificeAnalyzer::parsePatternLine(const std::string &line,
                                         SacrificePattern &pattern) {
  if (line.empty() || line[0] == '#')
    return false;

  std::istringstream iss(line);
  std::string priorityStr, ourPart, theirPart;

  if (!(iss >> priorityStr >> ourPart >> theirPart))
    return false;

  pattern.priority = std::atoi(priorityStr.c_str());
  pattern.ourConditions.clear();
  pattern.theirConditions.clear();

  // Parse our conditions (örnek: q+r*l*p3+)
  size_t i = 0;
  while (i < ourPart.size()) {
    if (ourPart[i] == 'q' || ourPart[i] == 'r' || ourPart[i] == 'l' ||
        ourPart[i] == 'p') {
      SacCondition cond;
      cond.piece = ourPart[i];
      cond.comparison = '*';
      cond.value = 0;
      cond.orMore = false;
      cond.orLess = false;
      i++;

      // Sayı var mı?
      if (i < ourPart.size() && isdigit(ourPart[i])) {
        cond.value = ourPart[i] - '0';
        i++;
      }

      // Operatör var mı?
      if (i < ourPart.size()) {
        if (ourPart[i] == '+') {
          cond.comparison = '>';
          cond.orMore = true;
          i++;
        } else if (ourPart[i] == '-') {
          cond.comparison = '<';
          cond.orLess = true;
          i++;
        } else if (ourPart[i] == '=') {
          cond.comparison = '=';
          i++;
        } else if (ourPart[i] == '*') {
          cond.comparison = '*';
          i++;
        }
      }

      pattern.ourConditions.push_back(cond);
    } else {
      i++;
    }
  }

  // Parse their conditions (örnek: q=r=l=p1>=)
  i = 0;
  while (i < theirPart.size()) {
    if (theirPart[i] == 'q' || theirPart[i] == 'r' || theirPart[i] == 'l' ||
        theirPart[i] == 'p') {
      SacCondition cond;
      cond.piece = theirPart[i];
      cond.comparison = '*';
      cond.value = 0;
      cond.orMore = false;
      cond.orLess = false;
      i++;

      // Sayı var mı?
      if (i < theirPart.size() && isdigit(theirPart[i])) {
        cond.value = theirPart[i] - '0';
        i++;
      }

      // Operatörler
      while (i < theirPart.size()) {
        if (theirPart[i] == '>') {
          cond.comparison = '>';
          i++;
          if (i < theirPart.size() && theirPart[i] == '=') {
            cond.orMore = true;
            i++;
          }
        } else if (theirPart[i] == '<') {
          cond.comparison = '<';
          i++;
          if (i < theirPart.size() && theirPart[i] == '=') {
            cond.orLess = true;
            i++;
          }
        } else if (theirPart[i] == '=') {
          cond.comparison = '=';
          i++;
        } else if (theirPart[i] == '*') {
          cond.comparison = '*';
          i++;
        } else {
          break;
        }
      }

      pattern.theirConditions.push_back(cond);
    } else {
      i++;
    }
  }

  return !pattern.ourConditions.empty() || !pattern.theirConditions.empty();
}

bool SacrificeAnalyzer::loadPatternFile(const std::string &path, int sacType) {
  std::ifstream file(path);
  if (!file.is_open())
    return false;

  std::string line;
  while (std::getline(file, line)) {
    // Remove carriage return if present
    if (!line.empty() && line.back() == '\r')
      line.pop_back();

    SacrificePattern pattern;
    pattern.sacType = sacType;

    if (parsePatternLine(line, pattern)) {
      if (sacType == SAC_QUEEN) {
        queenSacPatterns.push_back(pattern);
      } else if (sacType == SAC_PAWN) {
        pawnSacPatterns.push_back(pattern);
      } else {
        pieceSacPatterns.push_back(pattern);
      }
    }
  }

  return true;
}

bool SacrificeAnalyzer::loadPatterns(const std::string &sacrificeDir) {
  // Pawn sacrifice patterns
  loadPatternFile(sacrificeDir + "/1_pawnsac_white", SAC_PAWN);
  loadPatternFile(sacrificeDir + "/1_pawnsac_black", SAC_PAWN);
  loadPatternFile(sacrificeDir + "/2_pawnsac_white", SAC_PAWN);
  loadPatternFile(sacrificeDir + "/2_pawnsac_black", SAC_PAWN);
  loadPatternFile(sacrificeDir + "/3_pawnsac_white", SAC_PAWN);
  loadPatternFile(sacrificeDir + "/3_pawnsac_black", SAC_PAWN);
  loadPatternFile(sacrificeDir + "/c2_pawnsac_white", SAC_PAWN);
  loadPatternFile(sacrificeDir + "/c2_pawnsac_black", SAC_PAWN);
  loadPatternFile(sacrificeDir + "/c3_pawnsac_white", SAC_PAWN);
  loadPatternFile(sacrificeDir + "/c3_pawnsac_black", SAC_PAWN);
  loadPatternFile(sacrificeDir + "/c5_pawnsac_white", SAC_PAWN);
  loadPatternFile(sacrificeDir + "/c5_pawnsac_black", SAC_PAWN);

  // Queen sacrifice patterns
  loadPatternFile(sacrificeDir + "/queensac_white", SAC_QUEEN);
  loadPatternFile(sacrificeDir + "/queensac_black", SAC_QUEEN);
  loadPatternFile(sacrificeDir + "/cqueensac_white", SAC_QUEEN);
  loadPatternFile(sacrificeDir + "/cqueensac_black", SAC_QUEEN);

  loaded = true;
  return true;
}

MaterialCount SacrificeAnalyzer::countMaterial(const Board &b, int side) const {
  MaterialCount mc;
  int mult = (side == 0) ? 1 : -1;

  for (int sq = 0; sq < 64; sq++) {
    int piece = b.squares[sq];
    if (piece == 0)
      continue;

    int pieceSide = piece > 0 ? 0 : 1;
    if (pieceSide != side)
      continue;

    int pt = std::abs(piece);
    switch (pt) {
    case 1:
      mc.pawns++;
      break; // Pawn
    case 2:  // Knight
    case 3:  // Bishop
      mc.lightPieces++;
      break;
    case 4:
      mc.rooks++;
      break; // Rook
    case 5:
      mc.queens++;
      break; // Queen
    }
  }

  return mc;
}

int SacrificeAnalyzer::analyzeSacrifice(const Board &b, int side) const {
  if (!loaded)
    return 0;

  MaterialCount us = countMaterial(b, side);
  MaterialCount them = countMaterial(b, 1 - side);

  int materialDiff = us.total() - them.total();

  // Materyal dezavantajındaysak sacrifice araştır
  if (materialDiff >= 0)
    return 0;

  int bonus = 0;

  // Queen sacrifice check
  if (us.queens < them.queens) {
    for (const auto &pattern : queenSacPatterns) {
      if (pattern.matches(us, them)) {
        bonus = std::max(bonus, 300); // Büyük queen sacrifice bonusu
        break;
      }
    }
  }

  // Pawn sacrifice check
  if (us.pawns < them.pawns) {
    for (const auto &pattern : pawnSacPatterns) {
      if (pattern.matches(us, them)) {
        bonus = std::max(bonus, 80 + (them.pawns - us.pawns) * 30);
        break;
      }
    }
  }

  // Piece sacrifice check
  if (us.lightPieces < them.lightPieces || us.rooks < them.rooks) {
    for (const auto &pattern : pieceSacPatterns) {
      if (pattern.matches(us, them)) {
        bonus = std::max(bonus, 150);
        break;
      }
    }
  }

  return bonus;
}

bool SacrificeAnalyzer::hasAttackPotential(const Board &b, int side) const {
  MaterialCount us = countMaterial(b, side);

  // En az 1 vezir veya 2 kale veya vezir+kale kombinasyonu
  if (us.queens >= 1)
    return true;
  if (us.rooks >= 2)
    return true;
  if (us.rooks >= 1 && us.lightPieces >= 2)
    return true;

  return false;
}

int SacrificeAnalyzer::getSacrificeBonus(const Board &b, int side) const {
  int bonus = analyzeSacrifice(b, side);

  // Saldırı potansiyeli varsa bonusu artır
  if (bonus > 0 && hasAttackPotential(b, side)) {
    bonus = bonus * 3 / 2;
  }

  return bonus;
}
