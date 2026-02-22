#ifndef SEARCH_H
#define SEARCH_H

#include "board.h"

extern void initLMRtables();

int negamax(Board& board, int depth, int alpha, int beta, int ply);

Move getBestMove(Board& board, int maxDepth, int movetimeMs = -1, const std::vector<uint64_t>& positionHistory = {}, int ply = 0);


#endif
