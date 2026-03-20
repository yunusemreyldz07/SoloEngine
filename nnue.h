#ifndef NNUE_H
#define NNUE_H

#include <cstdint>
#include <string>

struct Board;

// NNUE network parameters loaded from file.
extern int16_t feature_weights[768 * 256];
extern int16_t feature_biases[256];
extern int16_t output_weights[256 * 2];
extern int16_t output_bias;

int clippedReLu(int16_t x);
int evaluate_nnue(int stm, const int16_t* acc_white, const int16_t* acc_black);
void update_feature(int16_t* acc, int feature_idx, bool is_add);
bool load_nnue(const std::string& filename);
void RefreshAccumulator(const Board& board, int16_t* acc_white, int16_t* acc_black);

#endif
