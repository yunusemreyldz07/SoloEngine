#ifndef SEARCH_H
#define SEARCH_H

#include "board.h"
#include <atomic>
#include <cstdint>
#include <vector>

extern char columns[];
extern Move killerMove[2][100];
extern int historyTable[64][64];
extern std::atomic<long long> nodeCount;

void resetNodeCounter();
long long getNodeCounter();

int scoreMove(const Board &board, const Move &move, int ply,
              const Move *ttMove);

int quiescence(Board &board, int alpha, int beta, int ply);
int negamax(Board &board, int depth, int alpha, int beta, int ply,
            std::vector<uint64_t> &history, std::vector<Move> &pvLine);

Move getBestMove(Board &board, int maxDepth, int movetimeMs = -1,
                 const std::vector<uint64_t> &history = {}, int ply = 0);

void request_stop_search();

#endif