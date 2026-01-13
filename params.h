#ifndef PARAMS_H
#define PARAMS_H

struct EngineParams {
  int pawnValue = 100;
  int knightValue = 320;
  int bishopValue = 330;
  int rookValue = 500;
  int queenValue = 900;

  int openingWeight = 100;
  int middlegameWeight = 100;
  int endgameWeight = 100;

  int pawnStructureWeight = 100;
  int mobilityWeight = 100;

  // Ana agresiflik parametresi (100=normal, 150=agresif, 200=ultra)
  int aggressiveness = 150; // AGRESIF varsayılan!

  // Ek agresif parametreler
  int attackWeight = 150;     // Saldırı ağırlığı
  int sacrificeBonus = 200;   // Sacrifice bonus çarpanı
  int kingDangerWeight = 120; // Kral tehdit ağırlığı
  int tempoValue = 20;        // Tempo değeri
  int hangingPenalty = 50;    // Asılı taş cezası

  int threads = 1;
  int hashSizeMB = 16;

  int randomness = 0;
};

extern EngineParams params;

#endif
