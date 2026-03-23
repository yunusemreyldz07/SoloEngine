#ifndef DATAGEN_H
#define DATAGEN_H

#include "board.h"
#include <string>
#include <vector>
#include <atomic>

constexpr int MAX_GAME_PLYS = 400;

extern std::atomic<uint64_t> total_fens_generated;
extern std::atomic<uint64_t> games_played_count;

void load_book(const std::string& filename);

// thread_id   : worker index (used for output filename)
// target_games: stop after this many games (0 = unlimited)
// soft_nodes  : soft node limit per position search
// use_book    : whether to load positions from book
// seed        : base RNG seed (thread_id is added to it for uniqueness)
void datagen_worker(int thread_id, uint64_t target_games, int soft_nodes, bool use_book, uint64_t seed);

void start_datagen(int num_threads, uint64_t target_games, int soft_nodes, bool use_book, uint64_t seed);

#endif
