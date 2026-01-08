#include "bitboard.h"
#include "types.h"

#include <iostream>
#include <cstdint>
#include <fstream>

#define WHITE 0
#define BLACK 1

#define set_bit(bitboard, square) ((bitboard) |= (1ULL << (square))) // Sets a bit to 1 at the given square

#define pop_bit(bitboard, square) ((bitboard) &= ~(1ULL << (square))) // Sets a bit to 0 at the given square

#define get_bit(bitboard, square) ((bitboard) & (1ULL << (square))) // Gets the bit at the given square

typedef uint64_t U64; // We will call unsigned 64-bit integers U64 

enum {
    a1, b1, c1, d1, e1, f1, g1, h1,
    a2, b2, c2, d2, e2, f2, g2, h2,
    a3, b3, c3, d3, e3, f3, g3, h3,
    a4, b4, c4, d4, e4, f4, g4, h4,
    a5, b5, c5, d5, e5, f5, g5, h5,
    a6, b6, c6, d6, e6, f6, g6, h6,
    a7, b7, c7, d7, e7, f7, g7, h7,
    a8, b8, c8, d8, e8, f8, g8, h8
};

int rook_relevant_bits[64] = {
    12, 11, 11, 11, 11, 11, 11, 12,
    11, 10, 10, 10, 10, 10, 10, 11,
    11, 10, 10, 10, 10, 10, 10, 11,
    11, 10, 10, 10, 10, 10, 10, 11,
    11, 10, 10, 10, 10, 10, 10, 11,
    11, 10, 10, 10, 10, 10, 10, 11,
    11, 10, 10, 10, 10, 10, 10, 11,
    12, 11, 11, 11, 11, 11, 11, 12
};

int bishop_relevant_bits[64] = {
    6, 5, 5, 5, 5, 5, 5, 6,
    5, 5, 5, 5, 5, 5, 5, 5,
    5, 5, 7, 7, 7, 7, 5, 5,
    5, 5, 7, 9, 9, 7, 5, 5,
    5, 5, 7, 9, 9, 7, 5, 5,
    5, 5, 7, 7, 7, 7, 5, 5,
    5, 5, 5, 5, 5, 5, 5, 5,
    6, 5, 5, 5, 5, 5, 5, 6
};
U64 pawn_attacks[2][64]; // [2 colors][64 squares]

U64 knight_attacks[64];

U64 king_attacks[64];

U64 queen_attacks[64];

U64 rook_attacks[64];

U64 bishop_attacks[64];

// Xorshift algorithm for random number generation
unsigned int random_state = 1804289383;

unsigned int get_random_U32() {
    unsigned int number = random_state;
    number ^= number << 13;
    number ^= number >> 17;
    number ^= number << 5;
    random_state = number;
    return number;
}

U64 get_random_U64() {
    U64 n1 = (U64)(get_random_U32()) & 0xFFFF;
    U64 n2 = (U64)(get_random_U32()) & 0xFFFF;
    U64 n3 = (U64)(get_random_U32()) & 0xFFFF;
    U64 n4 = (U64)(get_random_U32()) & 0xFFFF;
    return n1 | (n2 << 16) | (n3 << 32) | (n4 << 48);
}

// Generate a magic number candidate
// We AND three random U64 numbers to increase the chance of getting sparse numbers
U64 generate_magic_candidate() {
    return get_random_U64() & get_random_U64() & get_random_U64();
}

U64 set_occupancy(int index, int bits_in_mask, U64 attack_mask) {
    U64 occupancy = 0ULL;
    
    for (int count = 0; count < bits_in_mask; count++) {
        int square = lsb(attack_mask); // get the least significant bit index
        
        pop_bit(attack_mask, square);
        
        if (index & (1 << count)) {
            occupancy |= (1ULL << square);
        }
    }
    return occupancy;
}

U64 mask_pawn_attacks(int side, int square) { // side: 0 for white, 1 for black
    U64 attacks = 0ULL;

    int rank = square / 8;
    int file = square % 8;
    
    if (side == WHITE) {
        if (rank + 1 < 8 && file - 1 >= 0) set_bit(attacks, (rank + 1) * 8 + (file - 1));
        if (rank + 1 < 8 && file + 1 < 8)  set_bit(attacks, (rank + 1) * 8 + (file + 1));
    } 
    else {
        if (rank - 1 >= 0 && file - 1 >= 0) set_bit(attacks, (rank - 1) * 8 + (file - 1));
        if (rank - 1 >= 0 && file + 1 < 8)  set_bit(attacks, (rank - 1) * 8 + (file + 1));
    }
    
    return attacks;
}

U64 mask_knight_attacks(int square) {
    U64 attacks = 0ULL;
    int rank = square / 8;
    int file = square % 8;
    
    int dr[] = {-2, -2, -1, -1,  1, 1,  2, 2}; // direction rank
    int df[] = {-1,  1, -2,  2, -2, 2, -1, 1}; // direction file
    
    for (int i = 0; i < 8; i++) {
        int target_rank = rank + dr[i];
        int target_file = file + df[i];

        if (target_rank >= 0 && target_rank < 8 && target_file >= 0 && target_file < 8) {
            int target_square = target_rank * 8 + target_file;
            set_bit(attacks, target_square);
        }
    }
    return attacks;
}

U64 mask_king_attacks(int square) {
    U64 attacks = 0ULL;
    int rank = square / 8;
    int file = square % 8;
    
    int dr[] = {-1, -1, -1,  0, 0,  1, 1, 1};
    int df[] = {-1,  0,  1, -1, 1, -1, 0, 1};
    
    for (int i = 0; i < 8; i++) {
        int target_rank = rank + dr[i];
        int target_file = file + df[i];
        
        if (target_rank >= 0 && target_rank < 8 && target_file >= 0 && target_file < 8) {
            int target_square = target_rank * 8 + target_file;
            set_bit(attacks, target_square);
        }
    }
    return attacks;
}

U64 mask_rook_attacks(int square) {
    U64 attacks = 0ULL;
    int rank = square / 8;
    int file = square % 8;
    
    // north side 
    for (int r = rank + 1; r < 7; r++) set_bit(attacks, r * 8 + file);
    
    // south side
    for (int r = rank - 1; r > 0; r--) set_bit(attacks, r * 8 + file);
    
    // east side
    for (int f = file + 1; f < 7; f++) set_bit(attacks, rank * 8 + f);
    
    // west side
    for (int f = file - 1; f > 0; f--) set_bit(attacks, rank * 8 + f);
    
    return attacks;
}

U64 mask_bishop_attacks(int square) {
    U64 attacks = 0ULL;
    int rank = square / 8;
    int file = square % 8;
    
    // north-east
    for (int r = rank + 1, f = file + 1; r < 7 && f < 7; r++, f++) 
        set_bit(attacks, r * 8 + f);
        
    // south-east    
    for (int r = rank - 1, f = file + 1; r > 0 && f < 7; r--, f++) 
        set_bit(attacks, r * 8 + f);
    
    // south-west    
    for (int r = rank - 1, f = file - 1; r > 0 && f > 0; r--, f--) 
        set_bit(attacks, r * 8 + f);
    
    // north-west    
    for (int r = rank + 1, f = file - 1; r < 7 && f > 0; r++, f--) 
        set_bit(attacks, r * 8 + f);
    
    return attacks;
}

U64 rook_attacks_on_the_fly(int square, U64 block) {
    U64 attacks = 0ULL;
    int r, f;
    int tr = square / 8; // Target Rank
    int tf = square % 8; // Target File
    // an example so ppl who read this can understand it better. let's assume square = 12 (b5) 
    // tr = 12 / 8 = 1 (rank 2)
    // tf = 12 % 8 = 4 (file e)
    // r = tr + 1 = 1 + 1 = 2 (rank 3)
    // r * 8 + tf = 2 * 8 + 4 = 20 (c5)
    // north (r < 8)
    for (r = tr + 1; r < 8; r++) {
        set_bit(attacks, r * 8 + tf);
        // If there is a piece on the block, stop.
        if (get_bit(block, r * 8 + tf)) break;
    }
    
    // south (r >= 0)
    for (r = tr - 1; r >= 0; r--) {
        set_bit(attacks, r * 8 + tf);
        if (get_bit(block, r * 8 + tf)) break;
    }
    
    // east (f < 8)
    for (f = tf + 1; f < 8; f++) {
        set_bit(attacks, tr * 8 + f);
        if (get_bit(block, tr * 8 + f)) break;
    }
    
    // west (f >= 0)
    for (f = tf - 1; f >= 0; f--) {
        set_bit(attacks, tr * 8 + f);
        if (get_bit(block, tr * 8 + f)) break;
    }
    
    return attacks;
}

// Bishop's real moves calculation
U64 bishop_attacks_on_the_fly(int square, U64 block) {
    U64 attacks = 0ULL;
    int r, f;
    int tr = square / 8;
    int tf = square % 8;
    
    // north-east
    for (r = tr + 1, f = tf + 1; r < 8 && f < 8; r++, f++) {
        set_bit(attacks, r * 8 + f);
        if (get_bit(block, r * 8 + f)) break;
    }
    
    // south-east
    for (r = tr - 1, f = tf + 1; r >= 0 && f < 8; r--, f++) {
        set_bit(attacks, r * 8 + f);
        if (get_bit(block, r * 8 + f)) break;
    }
    
    // south-west
    for (r = tr - 1, f = tf - 1; r >= 0 && f >= 0; r--, f--) {
        set_bit(attacks, r * 8 + f);
        if (get_bit(block, r * 8 + f)) break;
    }
    
    // north-west
    for (r = tr + 1, f = tf - 1; r < 8 && f >= 0; r++, f--) {
        set_bit(attacks, r * 8 + f);
        if (get_bit(block, r * 8 + f)) break;
    }
    
    return attacks;
}

U64 rook_magic_numbers[64];
U64 bishop_magic_numbers[64];


U64 rook_attacks_table[64][4096];
U64 bishop_attacks_table[64][512];

constexpr uint32_t MAGIC_CACHE_VERSION = 2;
constexpr const char* MAGIC_CACHE_FILE = "magic_cache.bin";

static bool load_magic_cache() {
    std::ifstream in(MAGIC_CACHE_FILE, std::ios::binary);
    if (!in) return false;
    uint32_t version = 0;
    in.read(reinterpret_cast<char*>(&version), sizeof(version));
    if (!in || version != MAGIC_CACHE_VERSION) return false;
    in.read(reinterpret_cast<char*>(rook_magic_numbers), sizeof(rook_magic_numbers));
    in.read(reinterpret_cast<char*>(bishop_magic_numbers), sizeof(bishop_magic_numbers));
    return static_cast<bool>(in);
}

static void save_magic_cache() {
    std::ofstream out(MAGIC_CACHE_FILE, std::ios::binary | std::ios::trunc);
    if (!out) return;
    uint32_t version = MAGIC_CACHE_VERSION;
    out.write(reinterpret_cast<const char*>(&version), sizeof(version));
    out.write(reinterpret_cast<const char*>(rook_magic_numbers), sizeof(rook_magic_numbers));
    out.write(reinterpret_cast<const char*>(bishop_magic_numbers), sizeof(bishop_magic_numbers));
}

static bool validate_magic(int square, int relevant_bits, bool bishop, U64 magic) {
    if (magic == 0ULL) return false;
    U64 occupancies[4096];
    U64 attacks[4096];
    U64 used_attacks[4096];

    U64 attack_mask = bishop ? mask_bishop_attacks(square) : mask_rook_attacks(square);
    int occupancy_indices = 1 << relevant_bits;

    for (int index = 0; index < occupancy_indices; index++) {
        occupancies[index] = set_occupancy(index, relevant_bits, attack_mask);
        attacks[index] = bishop ? bishop_attacks_on_the_fly(square, occupancies[index])
                                : rook_attacks_on_the_fly(square, occupancies[index]);
    }

    for (int i = 0; i < 4096; i++) used_attacks[i] = 0ULL;

    for (int index = 0; index < occupancy_indices; index++) {
        int magic_index = (int)((occupancies[index] * magic) >> (64 - relevant_bits));
        if (used_attacks[magic_index] == 0ULL) {
            used_attacks[magic_index] = attacks[index];
        } else if (used_attacks[magic_index] != attacks[index]) {
            return false;
        }
    }

    return true;
}


U64 find_magic_number(int square, int relevant_bits, int bishop) {
    U64 occupancies[4096];
    U64 attacks[4096];
    U64 used_attacks[4096];

    U64 attack_mask = bishop ? mask_bishop_attacks(square) : mask_rook_attacks(square);
    
    int occupancy_indices = 1 << relevant_bits;
    
    for (int index = 0; index < occupancy_indices; index++) {
        occupancies[index] = set_occupancy(index, relevant_bits, attack_mask);
        
        if (bishop) 
            attacks[index] = bishop_attacks_on_the_fly(square, occupancies[index]);
        else 
            attacks[index] = rook_attacks_on_the_fly(square, occupancies[index]);
    }
    
    for (int i = 0; i < 100000000; i++) {
        U64 magic_candidate = generate_magic_candidate();
        
        if (__builtin_popcountll((attack_mask * magic_candidate) & 0xFF00000000000000ULL) < 6) continue;

        for (int j = 0; j < 4096; j++) used_attacks[j] = 0ULL;
        
        int fail = 0;
        
        for (int index = 0; index < occupancy_indices; index++) {
            
            int magic_index = (int)((occupancies[index] * magic_candidate) >> (64 - relevant_bits));
            
            if (used_attacks[magic_index] == 0ULL) {
                used_attacks[magic_index] = attacks[index];
            }
            else if (used_attacks[magic_index] != attacks[index]) {
                fail = 1;
                break;
            }
        }
        
        if (!fail) {
            return magic_candidate;
        }
    }
    // If no magic number is found, return 0
    return 0ULL;
}

void print_bitboard(U64 bitboard) {
    std::cout << "\n";
    for (int rank = 0; rank < 8; rank++) {
        std::cout << " " << (8 - rank) << "  ";
        for (int file = 0; file < 8; file++) {

            int square = (7 - rank) * 8 + file;

            if (get_bit(bitboard, square))
                std::cout << " 1 ";
            else
                std::cout << " . ";
        }
        std::cout << "\n";
    }
    std::cout << "\n     a  b  c  d  e  f  g  h\n\n";
    std::cout << "     Bitboard: " << bitboard << "\n\n";
}

void init_bitboards() {
    for (int square = 0; square < 64; square++) {
        pawn_attacks[0][square] = mask_pawn_attacks(0, square);
        pawn_attacks[1][square] = mask_pawn_attacks(1, square);

        knight_attacks[square] = mask_knight_attacks(square);
        king_attacks[square] = mask_king_attacks(square);

        rook_attacks[square] = mask_rook_attacks(square);
        bishop_attacks[square] = mask_bishop_attacks(square);
        queen_attacks[square] = rook_attacks[square] | bishop_attacks[square];
    }

    bool have_magics = load_magic_cache();
    bool cache_dirty = false;
    for (int square = 0; square < 64; square++) {
        if (!have_magics || !validate_magic(square, rook_relevant_bits[square], false, rook_magic_numbers[square])) {
            rook_magic_numbers[square] = find_magic_number(square, rook_relevant_bits[square], 0);
            cache_dirty = true;
        }
        if (!have_magics || !validate_magic(square, bishop_relevant_bits[square], true, bishop_magic_numbers[square])) {
            bishop_magic_numbers[square] = find_magic_number(square, bishop_relevant_bits[square], 1);
            cache_dirty = true;
        }
    }
    if (cache_dirty) save_magic_cache();

    for (int square = 0; square < 64; square++) {
        U64 mask = mask_rook_attacks(square);
        int bits = rook_relevant_bits[square];
        int count = 1 << bits;
        if (rook_magic_numbers[square] != 0ULL) {
            for (int i = 0; i < count; i++) {
                U64 occ = set_occupancy(i, bits, mask);
                int magic_idx = (int)((occ * rook_magic_numbers[square]) >> (64 - bits));
                rook_attacks_table[square][magic_idx] = rook_attacks_on_the_fly(square, occ);
            }
        }

        mask = mask_bishop_attacks(square);
        bits = bishop_relevant_bits[square];
        count = 1 << bits;
        if (bishop_magic_numbers[square] != 0ULL) {
            for (int i = 0; i < count; i++) {
                U64 occ = set_occupancy(i, bits, mask);
                int magic_idx = (int)((occ * bishop_magic_numbers[square]) >> (64 - bits));
                bishop_attacks_table[square][magic_idx] = bishop_attacks_on_the_fly(square, occ);
            }
        }
    }
}


U64 get_rook_attacks(int square, U64 occupancy) {
    if (rook_magic_numbers[square] == 0ULL) {
        return rook_attacks_on_the_fly(square, occupancy);
    }
    occupancy &= mask_rook_attacks(square);
    occupancy *= rook_magic_numbers[square];
    occupancy >>= 64 - rook_relevant_bits[square];
    return rook_attacks_table[square][occupancy];
}

U64 get_bishop_attacks(int square, U64 occupancy) {
    if (bishop_magic_numbers[square] == 0ULL) {
        return bishop_attacks_on_the_fly(square, occupancy);
    }
    occupancy &= mask_bishop_attacks(square);
    occupancy *= bishop_magic_numbers[square];
    occupancy >>= 64 - bishop_relevant_bits[square];
    return bishop_attacks_table[square][occupancy];
}
