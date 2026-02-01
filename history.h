#ifndef HISTORY_H
#define HISTORY_H

#include "board.h"

// History table: [fromSquare][toSquare]
// Each square is from 0-63, total 64x64 = 4096 entries
extern int historyTable[64][64];
extern Move killerMove[2][MAX_PLY]; // 2 slots

// Continuation history: [piece][toSq][piece][toSq]
// piece index = (pieceType - 1) + (color * 6), giving 0-11 for all pieces
// Tracks how good a move is based on the previous move played
extern int conhistTable[12][64][12][64];

// History functions
void clear_history();                          // Reset history table
void update_history(int fromSq, int toSq, int depth, const Move badQuiets[256], const int& badQuietCount); // Update on beta cutoff
int get_history_score(int fromSq, int toSq);  // Get score for move ordering
void add_killer_move(const Move& move, int ply); // Update killer moves
Move get_killer_move(int index, int ply); // Get killer moves
void clear_killer_moves(); // Clear killer moves

// Continuation history functions
void clear_conhist();
void update_conhist(const Move& prevMove, const Move& currMove, int depth, const Move badQuiets[256], int badQuietCount);
int get_conhist_score(const Move& prevMove, const Move& currMove);

// Helper to check if a move is a killer move at given ply
inline bool is_killer_move(const Move& move, int ply) {
    if (ply < 0 || ply >= MAX_PLY) return false;
    return moves_equal(move, get_killer_move(0, ply)) || 
           moves_equal(move, get_killer_move(1, ply));
}

#endif