#ifndef NNUE_H
#define NNUE_H

#include <array>
#include <cstdint>

struct Board;

inline bool USE_NNUE = true;

inline constexpr int NNUE_INPUT_SIZE = 768;
inline constexpr int NNUE_HIDDEN_SIZE = 1024;

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

inline void featureIndices(int piece, int sq, int& w_idx, int& b_idx) {
    w_idx = 64 * (piece - 1) + sq;
    b_idx = 64 * ((piece - 1 + 6) % 12) + (sq ^ 56);
}
void applyQuietBoth(Accumulator acc[2], int wAdd, int wSub, int bAdd, int bSub);
void applyCaptureBoth(Accumulator acc[2], int wAdd, int wSub1, int wSub2, int bAdd, int bSub1, int bSub2);
void applyCastlingBoth(Accumulator acc[2], int wAdd1, int wAdd2, int wSub1, int wSub2, int bAdd1, int bAdd2, int bSub1, int bSub2);

#endif
