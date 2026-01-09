#ifndef BITBOARD_H
#define BITBOARD_H
#include <cstdint>

using Bitboard = uint64_t;

inline constexpr int row_of(int sq) { return sq >> 3; }
inline constexpr int col_of(int sq) { return sq & 7; }
inline constexpr int square_of(int r, int c) { return (r << 3) | c; }
inline constexpr Bitboard square_bb(int sq) { return 1ULL << sq; }
inline int popcount(Bitboard b) { return __builtin_popcountll(b); }
inline int lsb(Bitboard b) { return __builtin_ctzll(b); }
inline int pop_lsb(Bitboard &b) {
  int s = lsb(b);
  b &= b - 1;
  return s;
}

extern Bitboard KnightAttacks[64];
extern Bitboard KingAttacks[64];
extern Bitboard PawnAttacks[2][64];
extern Bitboard RookMask[64];
extern Bitboard BishopMask[64];
extern Bitboard RookMagic[64];
extern Bitboard BishopMagic[64];
extern int RookShift[64];
extern int BishopShift[64];
extern Bitboard *RookAttackPtr[64];
extern Bitboard *BishopAttackPtr[64];

void init_bitboards();
Bitboard rook_attacks(int sq, Bitboard occ);
Bitboard bishop_attacks(int sq, Bitboard occ);
inline Bitboard queen_attacks(int sq, Bitboard occ) {
  return rook_attacks(sq, occ) | bishop_attacks(sq, occ);
}
inline Bitboard knight_attacks(int sq) { return KnightAttacks[sq]; }
inline Bitboard king_attacks(int sq) { return KingAttacks[sq]; }
inline Bitboard pawn_attacks(int side, int sq) { return PawnAttacks[side][sq]; }

#endif
