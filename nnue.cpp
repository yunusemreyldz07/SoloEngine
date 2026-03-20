#include "nnue.h"
#include <cstring>
#include "nnue_data.h"
#include <iostream>

#include "board.h"


// NNUE Weights and Biases
int16_t feature_weights[768 * 256];
int16_t feature_biases[256];
int16_t output_weights[256 * 2];
int16_t output_bias;

int32_t screlu(int16_t x) {
    int32_t y = x;
    if (y < 0) y = 0;
    if (y > 255) y = 255; // QA = 255
    return y * y;
}

int clippedReLu(int16_t x) {
    if (x < 0) return 0;
    if (x > 255) return 255; // Clipping to prevent overflow
    return x;
}

int evaluate_nnue(int stm, const int16_t* acc_white, const int16_t* acc_black) {
    const int16_t* us_acc = (stm == 0) ? acc_white : acc_black;
    const int16_t* them_acc = (stm == 0) ? acc_black : acc_white;

    int64_t sum = 0;

    for (int i = 0; i < 256; i++) {
        sum += screlu(us_acc[i]) * output_weights[i];
        sum += screlu(them_acc[i]) * output_weights[256 + i];
    }

    sum /= 255;
    sum += output_bias;
    sum *= 400; 
    sum /= (255 * 64);

    return (int)sum;
}

void update_feature(int16_t* acc, int feature_idx, bool is_add) {
    int weight_offset = feature_idx * 256;
    for (int i = 0; i < 256; i++) {
        if (is_add) {
            acc[i] += feature_weights[weight_offset + i];
        } else {
            acc[i] -= feature_weights[weight_offset + i];
        }
    }
}

void load_nnue() {
    size_t offset = 0;

    std::memcpy(feature_weights, nnue_data + offset, sizeof(feature_weights));
    offset += sizeof(feature_weights);

    std::memcpy(feature_biases, nnue_data + offset, sizeof(feature_biases));
    offset += sizeof(feature_biases);

    std::memcpy(output_weights, nnue_data + offset, sizeof(output_weights));
    offset += sizeof(output_weights);

    std::memcpy(&output_bias, nnue_data + offset, sizeof(output_bias));
}

void RefreshAccumulator(const Board& board, int16_t* acc_white, int16_t* acc_black) {
    for (int i = 0; i < 256; i++) {
        acc_white[i] = feature_biases[i];
        acc_black[i] = feature_biases[i];
    }

    for (int sq = 0; sq < 64; sq++) {
        int piece = board.mailbox[sq];
        
        if (piece != 0) {
            int white_idx = 64 * (piece - 1) + sq;
            update_feature(acc_white, white_idx, true);

            int black_idx = 64 * ((piece - 1 + 6) % 12) + (sq ^ 56);
            update_feature(acc_black, black_idx, true);
        }
    }
}