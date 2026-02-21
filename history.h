#ifndef HISTORY_H
#define HISTORY_H

#include "board.h"

// History table: [color][fromSquare][toSquare]
// Each square is from 0-63, total 64x64 = 4096 entries
extern int historyTable[2][64][64];
extern int CORRHIST_WEIGHT_SCALE;
extern int CORRHIST_GRAIN;
extern int CORRHIST_SIZE;
extern int CORRHIST_MAX;

extern int pawnCorrectionHistory[2][16384];
// History functions
void clear_history();                          // Reset history table
void update_history(int color, int fromSq, int toSq, int depth, const Move badQuiets[256], const int& badQuietCount); // Update on beta cutoff
int get_history_score(int color, int fromSq, int toSq);  // Get score for move ordering

int adjustEvalWithCorrectionHistory(Board *board, const int rawEval);
void updatePawnCorrectionHistory(Board *board, const int depth, const int diff);
void clearPawnCorrectionHistory();
uint64_t generatePawnKey(Board *board);

#endif
