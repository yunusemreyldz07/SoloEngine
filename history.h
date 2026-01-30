#ifndef HISTORY_H
#define HISTORY_H

#include "board.h"

// History table: [fromSquare][toSquare]
// Each square is from 0-63, total 64x64 = 4096 entries
extern int historyTable[64][64];
extern Move killerMove[2][100]; // 2 slots, max 100 ply

// Continuation History: [prevPieceIdx][prevToSq][currPieceIdx][currToSq]
// 12 piece types (6 white + 6 black), 64 squares each
extern int continuationHistory[12][64][12][64];

// History functions
void clear_history();                          // Reset all history tables
void update_history(int fromSq, int toSq, int depth, const Move badQuiets[256], const int& badQuietCount); // Update on beta cutoff
int get_history_score(int fromSq, int toSq);  // Get score for move ordering

// Continuation History functions
int get_continuation_history_score(const Board& board, const Move& currentMove);
void update_continuation_history(const Board& board, const Move& bestMove, int depth, const Move badQuiets[256], int badQuietCount);

// Killer move functions
void add_killer_move(const Move& move, int ply);
Move get_killer_move(int index, int ply);
void clear_killer_moves();

#endif