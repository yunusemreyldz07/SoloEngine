#ifndef TYPES_H
#define TYPES_H

#include <cstdint>

typedef uint64_t Bitboard;

inline int popcount(Bitboard b) {
    return __builtin_popcountll(b); // count how many bits are set to 1
}

inline int lsb(Bitboard b) {
    return __builtin_ctzll(b); // get the index of the least significant bit set to 1
}

#endif