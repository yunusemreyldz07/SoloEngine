#ifndef NNUE_H
#define NNUE_H

#include <array>
#include <cstdint>

struct Board;

inline constexpr bool USE_NNUE = false;

inline constexpr int NNUE_INPUT_SIZE = 768;
inline constexpr int NNUE_HIDDEN_SIZE = 64;

using Accumulator = std::array<int16_t, NNUE_HIDDEN_SIZE>;

// NNUE network parameters loaded from file.
extern int16_t hiddenWeight[NNUE_INPUT_SIZE * NNUE_HIDDEN_SIZE];
extern int16_t hiddenBias[NNUE_HIDDEN_SIZE];
extern int16_t outputWeight[NNUE_HIDDEN_SIZE * 2];
extern int16_t outputBias;

int32_t screlu(int16_t x);
void updateAccumulator(Accumulator& acc, int featureIdx, bool isAdd);
int makeFeatureIndex(int piece_type, int piece_color, int square, int perspective);
int evaluate_nnue(const Accumulator& acc_white, const Accumulator& acc_black, int side_to_move);
void load_nnue();
void RefreshAccumulator(const Board& board, Accumulator* acc_white, Accumulator* acc_black);

#endif
