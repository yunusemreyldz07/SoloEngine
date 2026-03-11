#ifndef HISTORY_H
#define HISTORY_H

#include "board.h"

struct MoveInfo {
    int piece; // 0-11 (colored piece index), -1 for invalid/null
    int to;    // 0-63, -1 for invalid
};

// History table: [color][fromSquare][toSquare]
extern int historyTable[2][64][64];
extern int conhistTable[12][64][12][64];  // 1-ply [prevPiece][prevTo][currPiece][currTo]
extern int conhistTable2[12][64][12][64]; // 2-ply [prevPiece][prevTo][currPiece][currTo]
extern MoveInfo moveStack[MAX_PLY];

void clear_history();
void reset_movestack();
void update_history(const Board& board, int color, int fromSq, int toSq, int depth, const Move badQuiets[256], const int& badQuietCount, int ply);
int get_history_score(int color, int fromSq, int toSq);
int get_conhist_score(int piece, int to, int ply);

#endif