#include "bitboard.h"
#include <vector>

Bitboard KnightAttacks[64], KingAttacks[64], PawnAttacks[2][64];
Bitboard RookMask[64], BishopMask[64], RookMagic[64], BishopMagic[64];
Bitboard *RookAttackPtr[64], *BishopAttackPtr[64];
int RookShift[64], BishopShift[64];
static std::vector<Bitboard> rookTable, bishopTable;

static Bitboard rook_slow(int sq, Bitboard occ) {
  Bitboard a = 0;
  int r = sq >> 3, f = sq & 7;
  for (int i = r + 1; i < 8; i++) {
    a |= 1ULL << (i * 8 + f);
    if (occ & (1ULL << (i * 8 + f)))
      break;
  }
  for (int i = r - 1; i >= 0; i--) {
    a |= 1ULL << (i * 8 + f);
    if (occ & (1ULL << (i * 8 + f)))
      break;
  }
  for (int i = f + 1; i < 8; i++) {
    a |= 1ULL << (r * 8 + i);
    if (occ & (1ULL << (r * 8 + i)))
      break;
  }
  for (int i = f - 1; i >= 0; i--) {
    a |= 1ULL << (r * 8 + i);
    if (occ & (1ULL << (r * 8 + i)))
      break;
  }
  return a;
}

static Bitboard bishop_slow(int sq, Bitboard occ) {
  Bitboard a = 0;
  int r = sq >> 3, f = sq & 7;
  for (int i = 1; r + i < 8 && f + i < 8; i++) {
    int t = (r + i) * 8 + f + i;
    a |= 1ULL << t;
    if (occ & (1ULL << t))
      break;
  }
  for (int i = 1; r + i < 8 && f - i >= 0; i++) {
    int t = (r + i) * 8 + f - i;
    a |= 1ULL << t;
    if (occ & (1ULL << t))
      break;
  }
  for (int i = 1; r - i >= 0 && f + i < 8; i++) {
    int t = (r - i) * 8 + f + i;
    a |= 1ULL << t;
    if (occ & (1ULL << t))
      break;
  }
  for (int i = 1; r - i >= 0 && f - i >= 0; i++) {
    int t = (r - i) * 8 + f - i;
    a |= 1ULL << t;
    if (occ & (1ULL << t))
      break;
  }
  return a;
}

static Bitboard idx_to_bb(int idx, int bits, Bitboard mask) {
  Bitboard r = 0;
  for (int i = 0; i < bits; i++) {
    int j = lsb(mask);
    mask &= mask - 1;
    if (idx & (1 << i))
      r |= 1ULL << j;
  }
  return r;
}

static uint64_t rng_state = 1804289383;
static Bitboard rng() {
  rng_state ^= rng_state << 13;
  rng_state ^= rng_state >> 7;
  rng_state ^= rng_state << 17;
  return rng_state;
}
static Bitboard rng_sparse() { return rng() & rng() & rng(); }

static Bitboard find_magic(int sq, int bits, bool bishop) {
  Bitboard mask = bishop ? BishopMask[sq] : RookMask[sq];
  int n = popcount(mask), size = 1 << n;
  Bitboard occ[4096], att[4096], used[4096];
  for (int i = 0; i < size; i++) {
    occ[i] = idx_to_bb(i, n, mask);
    att[i] = bishop ? bishop_slow(sq, occ[i]) : rook_slow(sq, occ[i]);
  }
  for (int k = 0; k < 100000000; k++) {
    Bitboard magic = rng_sparse();
    if (popcount((mask * magic) & 0xFF00000000000000ULL) < 6)
      continue;
    for (int i = 0; i < 4096; i++)
      used[i] = 0;
    bool fail = false;
    for (int i = 0; i < size && !fail; i++) {
      int idx = (int)((occ[i] * magic) >> (64 - bits));
      if (used[idx] == 0)
        used[idx] = att[i];
      else if (used[idx] != att[i])
        fail = true;
    }
    if (!fail)
      return magic;
  }
  return 0;
}

void init_bitboards() {
  const int kn_dr[] = {2, 2, 1, 1, -1, -1, -2, -2},
            kn_df[] = {1, -1, 2, -2, 2, -2, 1, -1};
  for (int sq = 0; sq < 64; sq++) {
    int r = sq >> 3, f = sq & 7;
    Bitboard b = 0;
    for (int i = 0; i < 8; i++) {
      int rr = r + kn_dr[i], ff = f + kn_df[i];
      if (rr >= 0 && rr < 8 && ff >= 0 && ff < 8)
        b |= 1ULL << (rr * 8 + ff);
    }
    KnightAttacks[sq] = b;
    b = 0;
    for (int rr = r - 1; rr <= r + 1; rr++)
      for (int ff = f - 1; ff <= f + 1; ff++)
        if (rr >= 0 && rr < 8 && ff >= 0 && ff < 8 && (rr != r || ff != f))
          b |= 1ULL << (rr * 8 + ff);
    KingAttacks[sq] = b;
    PawnAttacks[0][sq] = ((r > 0 && f > 0) ? 1ULL << (sq - 9) : 0) |
                         ((r > 0 && f < 7) ? 1ULL << (sq - 7) : 0);
    PawnAttacks[1][sq] = ((r < 7 && f > 0) ? 1ULL << (sq + 7) : 0) |
                         ((r < 7 && f < 7) ? 1ULL << (sq + 9) : 0);
  }
  for (int sq = 0; sq < 64; sq++) {
    int r = sq >> 3, f = sq & 7;
    Bitboard rm = 0, bm = 0;
    for (int i = r + 1; i < 7; i++)
      rm |= 1ULL << (i * 8 + f);
    for (int i = r - 1; i > 0; i--)
      rm |= 1ULL << (i * 8 + f);
    for (int i = f + 1; i < 7; i++)
      rm |= 1ULL << (r * 8 + i);
    for (int i = f - 1; i > 0; i--)
      rm |= 1ULL << (r * 8 + i);
    RookMask[sq] = rm;
    for (int i = 1; r + i < 7 && f + i < 7; i++)
      bm |= 1ULL << ((r + i) * 8 + f + i);
    for (int i = 1; r + i < 7 && f - i > 0; i++)
      bm |= 1ULL << ((r + i) * 8 + f - i);
    for (int i = 1; r - i > 0 && f + i < 7; i++)
      bm |= 1ULL << ((r - i) * 8 + f + i);
    for (int i = 1; r - i > 0 && f - i > 0; i++)
      bm |= 1ULL << ((r - i) * 8 + f - i);
    BishopMask[sq] = bm;
    RookShift[sq] = 64 - popcount(rm);
    BishopShift[sq] = 64 - popcount(bm);
  }
  rookTable.resize(0x100000);
  bishopTable.resize(0x1480);
  for (int sq = 0; sq < 64; sq++) {
    RookMagic[sq] = find_magic(sq, 64 - RookShift[sq], false);
    BishopMagic[sq] = find_magic(sq, 64 - BishopShift[sq], true);
  }
  int ro = 0;
  for (int sq = 0; sq < 64; sq++) {
    RookAttackPtr[sq] = &rookTable[ro];
    int bits = popcount(RookMask[sq]), size = 1 << bits;
    for (int i = 0; i < size; i++) {
      Bitboard occ = idx_to_bb(i, bits, RookMask[sq]);
      int idx = (int)((occ * RookMagic[sq]) >> RookShift[sq]);
      RookAttackPtr[sq][idx] = rook_slow(sq, occ);
    }
    ro += size;
  }
  int bo = 0;
  for (int sq = 0; sq < 64; sq++) {
    BishopAttackPtr[sq] = &bishopTable[bo];
    int bits = popcount(BishopMask[sq]), size = 1 << bits;
    for (int i = 0; i < size; i++) {
      Bitboard occ = idx_to_bb(i, bits, BishopMask[sq]);
      int idx = (int)((occ * BishopMagic[sq]) >> BishopShift[sq]);
      BishopAttackPtr[sq][idx] = bishop_slow(sq, occ);
    }
    bo += size;
  }
}

Bitboard rook_attacks(int sq, Bitboard occ) {
  return RookAttackPtr[sq]
                      [((occ & RookMask[sq]) * RookMagic[sq]) >> RookShift[sq]];
}
Bitboard bishop_attacks(int sq, Bitboard occ) {
  return BishopAttackPtr[sq][((occ & BishopMask[sq]) * BishopMagic[sq]) >>
                             BishopShift[sq]];
}
