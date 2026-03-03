#ifndef HISTORY_H
#define HISTORY_H

#include "board.h"

// History table: [color][fromSquare][toSquare]
// Each square is from 0-63, total 64x64 = 4096 entries
extern int historyTable[2][64][64];

// prevPieceType prevToSq curPieceType curToSq
// For continuation history heuristic (indexed by piece_type: 0-6)
extern int continuationHistoryTable[7][64][7][64];

// History functions
void clear_history();                          // Reset history table
void update_history(int color, int fromSq, int toSq, int depth, const Move badQuiets[256], const int& badQuietCount); // Update on beta cutoff
int get_history_score(int color, int fromSq, int toSq);  // Get score for move ordering
void updateContinuationHistory(int oldPiece, int oldToSq, int toPiece, int toSq, int bonus);
int getContinuationHistoryScore(int oldPiece, int oldToSq, int toPiece, int toSq);

#endif