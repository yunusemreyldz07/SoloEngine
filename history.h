#ifndef HISTORY_H
#define HISTORY_H

#include "board.h"

// History table: [color][fromSquare][toSquare]
// Each square is from 0-63, total 64x64 = 4096 entries
extern int historyTable[2][64][64];

// History functions
void clear_history();                          // Reset history table
void update_history(int color, int fromSq, int toSq, int depth, const Move badQuiets[256], const int& badQuietCount); // Update on beta cutoff
int get_history_score(int color, int fromSq, int toSq);  // Get score for move ordering

// Pawn correction history
extern int correctionHistoryWeightScale;
extern int correctionHistoryGrain;
extern int correctionHistorySize;
extern int correctionHistoryMax;

extern int pawnCorrectionHistory[2][16384];
void updatePawnCorrectionHistory(Board& board, const int& depth, const int& diff);
int adjustEvalWithCorrectionHistory(Board& board, int rawEval);

#endif