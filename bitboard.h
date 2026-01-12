#ifndef BITBOARD_H
#define BITBOARD_H

#include "types.h"

#define U64 uint64_t 
void init_all(); // Initialize bitboard lookup tables in the beginning of the program so we can use later
void init_bitboards();

// sq: which square (0-63)
// occ: occupancy of bitboard map
//Bitboard rook_attacks(int sq, Bitboard occ); // Rook attacks for the given square and occupancy

//Bitboard bishop_attacks(int sq, Bitboard occ); // Bishop attacks for the given square and occupancy

//Bitboard queen_attacks(int sq, Bitboard occ); // Queen attacks for the given square and occupancy

extern U64 pawn_attacks[2][64]; // [2 colors][64 squares]
extern U64 knight_attacks[64];
extern U64 king_attacks[64];

U64 get_rook_attacks(int square, U64 occupancy);
U64 get_bishop_attacks(int square, U64 occupancy);
U64 rook_attacks_on_the_fly(int square, U64 block);
U64 bishop_attacks_on_the_fly(int square, U64 block);
#endif
