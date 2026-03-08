#ifndef EVALUATION_H
#define EVALUATION_H

#include "board.h"
// Evaluation functions
int evaluate_board(const Board& board, bool include_pawn_penalties = true);
void evaluate_pawn_penalties(const Board& board, int mg[2], int eg[2]);

#endif