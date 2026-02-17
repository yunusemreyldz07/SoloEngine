#ifndef FEN_H
#define FEN_H

#include "board.h"
#include <string>
#include <cstdint>

std::string get_fen(const Board& board);
void generate_fen(Board board, int current_ply);
void set_fen_random_seed(uint32_t seed);
uint64_t get_random_u64_number();

#endif
