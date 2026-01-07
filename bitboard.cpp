#include "bitboard.h"
#include <cstring>
#include <iostream>
#include <vector>

Bitboard KnightAttacks[64];
Bitboard KingAttacks[64];
Bitboard PawnAttacks[2][64];
Bitboard LineBB[64][64];
Bitboard BetweenBB[64][64];

Bitboard RookMagic[64];
Bitboard BishopMagic[64];
int RookShift[64] = {52, 53, 53, 53, 53, 53, 53, 52, 53, 54, 54, 54, 54,
                     54, 54, 53, 53, 54, 54, 54, 54, 54, 54, 53, 53, 54,
                     54, 54, 54, 54, 54, 53, 53, 54, 54, 54, 54, 54, 54,
                     53, 53, 54, 54, 54, 54, 54, 54, 53, 53, 54, 54, 54,
                     54, 54, 54, 53, 52, 53, 53, 53, 53, 53, 53, 52};
int BishopShift[64] = {58, 59, 59, 59, 59, 59, 59, 58, 59, 59, 59, 59, 59,
                       59, 59, 59, 59, 59, 57, 57, 57, 57, 59, 59, 59, 59,
                       57, 55, 55, 57, 59, 59, 59, 59, 57, 55, 55, 57, 59,
                       59, 59, 59, 57, 57, 57, 57, 59, 59, 59, 59, 59, 59,
                       59, 59, 59, 59, 58, 59, 59, 59, 59, 59, 59, 58};

Bitboard RookMask[64], BishopMask[64];
Bitboard *RookAttackPtr[64];
Bitboard *BishopAttackPtr[64];

static std::vector<Bitboard> rookTable;
static std::vector<Bitboard> bishopTable;

static Bitboard get_rook_mask(int sq) {
  Bitboard r = 0;
  int rk = sq >> 3, fl = sq & 7;
  for (int i = rk + 1; i < 7; i++)
    r |= 1ULL << (i * 8 + fl);
  for (int i = rk - 1; i > 0; i--)
    r |= 1ULL << (i * 8 + fl);
  for (int i = fl + 1; i < 7; i++)
    r |= 1ULL << (rk * 8 + i);
  for (int i = fl - 1; i > 0; i--)
    r |= 1ULL << (rk * 8 + i);
  return r;
}

static Bitboard get_bishop_mask(int sq) {
  Bitboard r = 0;
  int rk = sq >> 3, fl = sq & 7;
  for (int i = 1; rk + i < 7 && fl + i < 7; i++)
    r |= 1ULL << ((rk + i) * 8 + fl + i);
  for (int i = 1; rk + i < 7 && fl - i > 0; i++)
    r |= 1ULL << ((rk + i) * 8 + fl - i);
  for (int i = 1; rk - i > 0 && fl + i < 7; i++)
    r |= 1ULL << ((rk - i) * 8 + fl + i);
  for (int i = 1; rk - i > 0 && fl - i > 0; i++)
    r |= 1ULL << ((rk - i) * 8 + fl - i);
  return r;
}

static Bitboard rook_attacks_slow(int sq, Bitboard block) {
  Bitboard a = 0;
  int r = sq >> 3, f = sq & 7;
  for (int i = r + 1; i < 8; i++) {
    a |= 1ULL << (i * 8 + f);
    if (block & (1ULL << (i * 8 + f)))
      break;
  }
  for (int i = r - 1; i >= 0; i--) {
    a |= 1ULL << (i * 8 + f);
    if (block & (1ULL << (i * 8 + f)))
      break;
  }
  for (int i = f + 1; i < 8; i++) {
    a |= 1ULL << (r * 8 + i);
    if (block & (1ULL << (r * 8 + i)))
      break;
  }
  for (int i = f - 1; i >= 0; i--) {
    a |= 1ULL << (r * 8 + i);
    if (block & (1ULL << (r * 8 + i)))
      break;
  }
  return a;
}

static Bitboard bishop_attacks_slow(int sq, Bitboard block) {
  Bitboard a = 0;
  int r = sq >> 3, f = sq & 7;
  for (int i = 1; r + i < 8 && f + i < 8; i++) {
    a |= 1ULL << ((r + i) * 8 + f + i);
    if (block & (1ULL << ((r + i) * 8 + f + i)))
      break;
  }
  for (int i = 1; r + i < 8 && f - i >= 0; i++) {
    a |= 1ULL << ((r + i) * 8 + f - i);
    if (block & (1ULL << ((r + i) * 8 + f - i)))
      break;
  }
  for (int i = 1; r - i >= 0 && f + i < 8; i++) {
    a |= 1ULL << ((r - i) * 8 + f + i);
    if (block & (1ULL << ((r - i) * 8 + f + i)))
      break;
  }
  for (int i = 1; r - i >= 0 && f - i >= 0; i++) {
    a |= 1ULL << ((r - i) * 8 + f - i);
    if (block & (1ULL << ((r - i) * 8 + f - i)))
      break;
  }
  return a;
}

static Bitboard index_to_u64(int idx, int bits, Bitboard m) {
  Bitboard r = 0;
  for (int i = 0; i < bits; i++) {
    int j = lsb(m);
    m &= m - 1;
    if (idx & (1 << i))
      r |= 1ULL << j;
  }
  return r;
}

static Bitboard random_u64() {
  static Bitboard s = 1804289383;
  Bitboard x = s;
  x ^= x << 13;
  x ^= x >> 7;
  x ^= x << 17;
  s = x;
  return x;
}

static Bitboard random_u64_fewbits() {
  return random_u64() & random_u64() & random_u64();
}

static Bitboard find_magic(int sq, int m, bool is_bishop) {
  Bitboard mask = is_bishop ? BishopMask[sq] : RookMask[sq];
  int bits = popcount(mask);
  int size = 1 << bits;
  Bitboard occ[4096];
  Bitboard att[4096];
  Bitboard used[4096];

  for (int i = 0; i < size; i++) {
    occ[i] = index_to_u64(i, bits, mask);
    att[i] = is_bishop ? bishop_attacks_slow(sq, occ[i])
                       : rook_attacks_slow(sq, occ[i]);
  }

  for (int k = 0; k < 100000000; k++) {
    Bitboard magic = random_u64_fewbits();
    if (popcount((mask * magic) & 0xFF00000000000000ULL) < 6)
      continue;

    for (int i = 0; i < 4096; i++)
      used[i] = 0;
    bool fail = false;

    for (int i = 0; i < size; i++) {
      int idx = (int)((occ[i] * magic) >> (64 - m));
      if (used[idx] == 0)
        used[idx] = att[i];
      else if (used[idx] != att[i]) {
        fail = true;
        break;
      }
    }
    if (!fail)
      return magic;
  }
  return 0;
}

static void init_piece_attacks() {
  for (int sq = 0; sq < 64; sq++) {
    int r = sq >> 3, f = sq & 7;

    Bitboard b = 0;
    int dr[] = {2, 2, 1, 1, -1, -1, -2, -2};
    int df[] = {1, -1, 2, -2, 2, -2, 1, -1};
    for (int i = 0; i < 8; i++) {
      int rr = r + dr[i], ff = f + df[i];
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

    PawnAttacks[0][sq] = ((r < 7 && f > 0) ? 1ULL << (sq + 7) : 0) |
                         ((r < 7 && f < 7) ? 1ULL << (sq + 9) : 0);
    PawnAttacks[1][sq] = ((r > 0 && f > 0) ? 1ULL << (sq - 9) : 0) |
                         ((r > 0 && f < 7) ? 1ULL << (sq - 7) : 0);
  }
}

static void init_line_between() {
  for (int s1 = 0; s1 < 64; s1++) {
    for (int s2 = 0; s2 < 64; s2++) {
      LineBB[s1][s2] = 0;
      BetweenBB[s1][s2] = 0;
      if (s1 == s2)
        continue;
      Bitboard ra = rook_attacks_slow(s1, 0);
      Bitboard ba = bishop_attacks_slow(s1, 0);
      if (ra & (1ULL << s2)) {
        LineBB[s1][s2] = (rook_attacks_slow(s1, 0) & rook_attacks_slow(s2, 0)) |
                         (1ULL << s1) | (1ULL << s2);
        BetweenBB[s1][s2] = rook_attacks_slow(s1, 1ULL << s2) &
                            rook_attacks_slow(s2, 1ULL << s1);
      } else if (ba & (1ULL << s2)) {
        LineBB[s1][s2] =
            (bishop_attacks_slow(s1, 0) & bishop_attacks_slow(s2, 0)) |
            (1ULL << s1) | (1ULL << s2);
        BetweenBB[s1][s2] = bishop_attacks_slow(s1, 1ULL << s2) &
                            bishop_attacks_slow(s2, 1ULL << s1);
      }
    }
  }
}

static void init_magics() {
  for (int sq = 0; sq < 64; sq++) {
    RookMask[sq] = get_rook_mask(sq);
    BishopMask[sq] = get_bishop_mask(sq);
  }

  rookTable.resize(0x100000);
  bishopTable.resize(0x1480);

  std::cout << "Generating Magics... " << std::flush;
  for (int sq = 0; sq < 64; sq++) {
    RookMagic[sq] = find_magic(sq, 64 - RookShift[sq], false);
    BishopMagic[sq] = find_magic(sq, 64 - BishopShift[sq], true);
  }
  std::cout << "Done." << std::endl;

  int roffset = 0;
  for (int sq = 0; sq < 64; sq++) {
    RookAttackPtr[sq] = &rookTable[roffset];
    Bitboard mask = RookMask[sq];
    int bits = popcount(mask);
    int size = 1 << bits;
    for (int i = 0; i < size; i++) {
      Bitboard occ = index_to_u64(i, bits, mask);
      int idx = (int)((occ * RookMagic[sq]) >> RookShift[sq]);
      RookAttackPtr[sq][idx] = rook_attacks_slow(sq, occ);
    }
    roffset += (1 << (64 - RookShift[sq]));
  }

  int boffset = 0;
  for (int sq = 0; sq < 64; sq++) {
    BishopAttackPtr[sq] = &bishopTable[boffset];
    Bitboard mask = BishopMask[sq];
    int bits = popcount(mask);
    int size = 1 << bits;
    for (int i = 0; i < size; i++) {
      Bitboard occ = index_to_u64(i, bits, mask);
      int idx = (int)((occ * BishopMagic[sq]) >> BishopShift[sq]);
      BishopAttackPtr[sq][idx] = bishop_attacks_slow(sq, occ);
    }
    boffset += (1 << (64 - BishopShift[sq]));
  }
}

void init_bitboards() {
  init_piece_attacks();
  init_line_between();
  init_magics();
}

Bitboard rook_attacks(int sq, Bitboard occ) {
  return RookAttackPtr[sq]
                      [((occ & RookMask[sq]) * RookMagic[sq]) >> RookShift[sq]];
}

Bitboard bishop_attacks(int sq, Bitboard occ) {
  return BishopAttackPtr[sq][((occ & BishopMask[sq]) * BishopMagic[sq]) >>
                             BishopShift[sq]];
}

Bitboard queen_attacks(int sq, Bitboard occ) {
  return rook_attacks(sq, occ) | bishop_attacks(sq, occ);
}
