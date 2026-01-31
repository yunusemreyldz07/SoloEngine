#ifndef HISTORY_H
#define HISTORY_H

#include "board.h"

// History table: [fromSquare][toSquare]
// Each square is from 0-63, total 64x64 = 4096 entries
extern int historyTable[64][64];
extern Move killerMove[2][100]; // 2 slots, max 100 ply

// Continuation History
struct SearchStackEntry {
    int piece;   // Moving piece type (1-6: pawn-king)
    int toSq;    // Destination square (0-63)
};
extern SearchStackEntry searchStack[128];
extern int contHist[6][64][6][64]; // [prevPiece][prevTo][currPiece][currTo]

// History functions
void clear_history();
void update_history(int fromSq, int toSq, int depth, const Move badQuiets[256], const int& badQuietCount);
int get_history_score(int fromSq, int toSq);
void add_killer_move(const Move& move, int ply);
Move get_killer_move(int index, int ply);
void clear_killer_moves();

// Continuation History functions
int get_conthist_score(int ply, int piece, int to);
void update_conthist(int ply, int piece, int to, int bonus);

#endif