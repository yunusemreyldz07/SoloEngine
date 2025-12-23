#ifndef SEARCH_H
#define SEARCH_H

#include "board.h"
#include <vector>
#include <cstdint>

// Move ordering
int scoreMove(const Board& board, const Move& move);

// Search functions
int quiescence(Board& board, int alpha, int beta, int ply, std::vector<uint64_t>& history);
int minimax(Board& board, int depth, int alpha, int beta, int ply, std::vector<uint64_t>& history);
Move getBestMove(Board& board, int depth, const std::vector<uint64_t>& baseHistory);

#endif