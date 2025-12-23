#ifndef EVALUATION_H
#define EVALUATION_H

#include "board.h"

// Piece values
extern const int PIECE_VALUES[7];

// Piece-square tables
extern const int pawn_pst[8][8];
extern const int knight_pst[8][8];
extern const int bishop_pst[8][8];
extern const int rook_pst[8][8];
extern const int queen_pst[8][8];
extern const int mg_king_pst[8][8];
extern const int eg_king_pst[8][8];

// Evaluation functions
bool is_endgame(const Board& board);
int evaulate_board(const Board& board);
int repetition_draw_score(const Board& board);

#endif