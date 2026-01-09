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
  int aggressiveness = 100;

  int threads = 1;
  int hashSizeMB = 16;
};

extern EngineParams params;

#endif
