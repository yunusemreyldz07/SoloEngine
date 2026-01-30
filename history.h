#ifndef HISTORY_H
#define HISTORY_H

#include "board.h"

// History table: [fromSquare][toSquare]
// Each square is from 0-63, total 64x64 = 4096 entries
extern int historyTable[64][64];
extern Move killerMove[2][100]; // 2 slots, max 100 ply

// History functions

void update_continuation_history(const Board& board, const Move& currentMove, int depth); // Update continuation history
int get_continuation_history_score(const Board& board, const Move& currentMove); //

void clear_history();                          // Reset history table
void update_history(int fromSq, int toSq, int depth, const Move badQuiets[64], const int& badQuietCount); // Update on beta cutoff
int get_history_score(int fromSq, int toSq);  // Get score for move ordering
void add_killer_move(const Move& move, int ply); // Update killer moves
Move get_killer_move(int index, int ply); // Get killer moves
void clear_killer_moves(); // Clear killer moves

#endif