#ifndef BITBOARD_H
#define BITBOARD_H

#include "types.h"

#define U64 unsigned long long // We will call unsigned 64-bit integers U64 
#define BISHOP 0
#define ROOK 1
extern U64 pawn_attacks[2][64]; // [side][squares]
extern U64 knight_attacks[64];
extern U64 king_attacks[64];
extern U64 bishop_masks[64];
extern U64 rook_masks[64];
extern U64 bishop_attacks[64][512];
extern U64 rook_attacks[64][4096];
void init_leapers_attack();
void init_slider_attacks(int piece);
U64 get_bishop_attacks(int square, U64 occupancy);
U64 get_rook_attacks(int square, U64 occupancy);
U64 get_queen_attacks(int square, U64 occupancy);
U64 bishop_attacks_otf(int square, U64 block);
U64 rook_attacks_otf(int square, U64 block);
void init_all();

#endif
