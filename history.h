#ifndef HISTORY_H
#define HISTORY_H

#include "board.h"

// History table: [fromSquare][toSquare]
// Each square is from 0-63, total 64x64 = 4096 entries
extern int historyTable[64][64];
extern Move killerMove[2][100]; // 2 slots, max 100 ply
extern uint16_t countinationHistory[12][64][12][64]; // [piece1][toSquare1][piece2][toSquare2]

// History functions
int piece_to_index(int piece); // Convert piece to index for countination history


uint16_t get_countination_score(int piece1, int toSquare1, int piece2, int toSquare2); // Get countination history score
void clear_countination_history(); // Clear countination history
void update_countination_history(int piece1, int toSquare1, int piece2, int toSquare2, int depth); // Update countination history
uint16_t calculate_countination_score(int ply, int offset, const Move& move, const Move* prevMove); // Calculate countination score for move ordering


void clear_history();                          // Reset history table
void update_history(int fromSq, int toSq, int depth); // Update on beta cutoff
int get_history_score(int fromSq, int toSq);  // Get score for move ordering


void add_killer_move(const Move& move, int ply); // Update killer moves
Move get_killer_move(int index, int ply); // Get killer moves
void clear_killer_moves(); // Clear killer moves

#endif
