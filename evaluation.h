#ifndef EVALUATION_H
#define EVALUATION_H

#include "board.h"
// Evaluation functions
int evaluate_board(const Board& board, bool include_pawn_penalties = true, bool include_psqt = true);
void evaluate_pawn_penalties(const Board& board, int mg[2], int eg[2]);
extern const int mg_value[6];
extern const int eg_value[6];
extern const int* mg_pesto_tables[6];
extern const int* eg_pesto_tables[6];
#endif