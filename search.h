#ifndef SEARCH_H
#define SEARCH_H

#include "board.h"
#include <vector>
#include <cstdint>
#include <atomic>
extern char columns[];
extern Move killerMove[2][100]; // 2 slots, max 100 ply
extern int historyTable[64][64]; // fromSquare x toSquare
extern std::atomic<long long> nodeCount; // visited node counter

void resetNodeCounter();
long long getNodeCounter();

// Move ordering
int scoreMove(const Board& board, const Move& move, int ply, const Move* ttMove);

// Search functions (PV enabled)
int quiescence(Board& board, int alpha, int beta, int ply);
int negamax(Board& board, int depth, int alpha, int beta, int ply, std::vector<uint64_t>& history, std::vector<Move>& pvLine);

// movetimeMs > 0: time-limited, effectively unlimited depth (search until time runs out).
// movetimeMs <= 0: depth-limited, no time limit.
Move getBestMove(Board& board, int maxDepth, int movetimeMs = -1, const std::vector<uint64_t>& history = {}, int ply = 0);

// Request the current search to stop as soon as possible.
// Safe to call even if no search is running.
void request_stop_search();

#endif