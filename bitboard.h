#ifndef BITBOARD_H
#define BITBOARD_H

#include <cstdint>

using Bitboard = uint64_t;

constexpr Bitboard FileA = 0x0101010101010101ULL;
constexpr Bitboard FileB = 0x0202020202020202ULL;
constexpr Bitboard FileC = 0x0404040404040404ULL;
constexpr Bitboard FileD = 0x0808080808080808ULL;
constexpr Bitboard FileE = 0x1010101010101010ULL;
constexpr Bitboard FileF = 0x2020202020202020ULL;
constexpr Bitboard FileG = 0x4040404040404040ULL;
constexpr Bitboard FileH = 0x8080808080808080ULL;

constexpr Bitboard Rank1 = 0xFF00000000000000ULL;
constexpr Bitboard Rank2 = 0x00FF000000000000ULL;
constexpr Bitboard Rank3 = 0x0000FF0000000000ULL;
constexpr Bitboard Rank4 = 0x000000FF00000000ULL;
constexpr Bitboard Rank5 = 0x00000000FF000000ULL;
constexpr Bitboard Rank6 = 0x0000000000FF0000ULL;
constexpr Bitboard Rank7 = 0x000000000000FF00ULL;
constexpr Bitboard Rank8 = 0x00000000000000FFULL;

constexpr Bitboard NotFileA = ~FileA;
constexpr Bitboard NotFileH = ~FileH;
constexpr Bitboard NotFileAB = ~(FileA | FileB);
constexpr Bitboard NotFileGH = ~(FileG | FileH);

inline int square_of(int row, int col) { return row * 8 + col; }
inline int row_of(int sq) { return sq / 8; }
inline int col_of(int sq) { return sq % 8; }
inline Bitboard square_bb(int sq) { return 1ULL << sq; }

inline int popcount(Bitboard b) {
#if defined(__GNUC__) || defined(__clang__)
  return __builtin_popcountll(b);
#elif defined(_MSC_VER)
  return (int)__popcnt64(b);
#else
  int count = 0;
  while (b) {
    count++;
    b &= b - 1;
  }
  return count;
#endif
}

inline int lsb(Bitboard b) {
#if defined(__GNUC__) || defined(__clang__)
  return __builtin_ctzll(b);
#elif defined(_MSC_VER)
  unsigned long idx;
  _BitScanForward64(&idx, b);
  return (int)idx;
#else
  int idx = 0;
  while (!(b & 1)) {
    b >>= 1;
    idx++;
  }
  return idx;
#endif
}

inline int pop_lsb(Bitboard &b) {
  int sq = lsb(b);
  b &= b - 1;
  return sq;
}

extern Bitboard KnightAttacks[64];
extern Bitboard KingAttacks[64];
extern Bitboard PawnAttacks[2][64];
extern Bitboard LineBB[64][64];
extern Bitboard BetweenBB[64][64];

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
Bitboard queen_attacks(int sq, Bitboard occ);

inline Bitboard knight_attacks(int sq) { return KnightAttacks[sq]; }
inline Bitboard king_attacks(int sq) { return KingAttacks[sq]; }
inline Bitboard pawn_attacks(int side, int sq) { return PawnAttacks[side][sq]; }

inline Bitboard shift_north(Bitboard b) { return b >> 8; }
inline Bitboard shift_south(Bitboard b) { return b << 8; }
inline Bitboard shift_east(Bitboard b) { return (b << 1) & NotFileA; }
inline Bitboard shift_west(Bitboard b) { return (b >> 1) & NotFileH; }
inline Bitboard shift_ne(Bitboard b) { return (b >> 7) & NotFileA; }
inline Bitboard shift_nw(Bitboard b) { return (b >> 9) & NotFileH; }
inline Bitboard shift_se(Bitboard b) { return (b << 9) & NotFileA; }
inline Bitboard shift_sw(Bitboard b) { return (b << 7) & NotFileH; }

#endif
