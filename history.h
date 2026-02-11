#ifndef HISTORY_H
#define HISTORY_H

#include "board.h"

// History table: [fromSquare][toSquare]
// Each square is from 0-63, total 64x64 = 4096 entries
extern int historyTable[64][64];
extern int conhistTable[12][64][12][64]; // continuation history [prevpiece][to][piece][to]
// History functions
void update_conhist(int prevPiece, int prevTo, int piece, int to, int depth); // Update continuation history on beta cutoff
int get_conhist_score(int prevPiece, int prevTo, int piece, int to); // Get continuation history score for move ordering
void clear_history();                          // Reset history table
void update_history(int fromSq, int toSq, int depth, const Move badQuiets[256], const int& badQuietCount); // Update on beta cutoff
int get_history_score(int fromSq, int toSq);  // Get score for move ordering

#endif