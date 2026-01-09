#ifndef SEARCH_H
#define SEARCH_H
#include "board.h"

constexpr int MAX_PLY = 128;

extern Move killerMoves[MAX_PLY][2];
extern int historyTable[64][64];
extern Move counterMoves[64][64];

void resetNodes();
long long getNodes();
void requestStop();
Move search(Board &b, int maxDepth, int timeMs,
            const std::vector<uint64_t> &history);
extern int uciThreads;

#endif