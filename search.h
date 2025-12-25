#ifndef SEARCH_H
#define SEARCH_H

#include "board.h"
#include <vector>
#include <cstdint>

extern Move killerMove[2][100]; // 2 slots, max 100 ply
extern int historyTable[64][64]; // fromSquare x toSquare

// Move ordering
int scoreMove(const Board& board, const Move& move, int ply);

// Search functions
int quiescence(Board& board, int alpha, int beta, int ply, std::vector<uint64_t>& history);
int minimax(Board& board, int depth, int alpha, int beta, int ply, std::vector<uint64_t>& history);
Move getBestMove(Board& board, int depth, const std::vector<uint64_t>& baseHistory, int wtime, int btime, int winc, int binc);

#endif