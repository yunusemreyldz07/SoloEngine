#ifndef HISTORY_H
#define HISTORY_H

#include "board.h"

struct MoveInfo {
    int piece; // 0-11 (colored piece index), -1 for invalid/null
    int to;    // 0-63, -1 for invalid
};

// History table: [color][fromSquare][toSquare]
extern int historyTable[2][64][64];
extern int conhistTable[12][64][12][64]; // [prevPiece][prevTo][currPiece][currTo]

// Pawn Correction History
constexpr int CORRHIST_SIZE = 16384;
extern int corrhistTable[2][CORRHIST_SIZE]; // [color][pawnKeyIndex]
extern thread_local MoveInfo moveStack[MAX_PLY];

void clear_history();
void reset_movestack();
void update_history(const Board& board, int color, int fromSq, int toSq, int depth, const Move badQuiets[256], const int& badQuietCount, int ply);
int get_history_score(int color, int fromSq, int toSq);
int get_conhist_score(int piece, int to, int ply);

void update_corrhist(const Board& board, int stm, int16_t bestEval, int16_t rawStaticEval);
int  get_corrhist_adj(const Board& board, int stm);

#endif