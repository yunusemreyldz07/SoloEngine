#include "nnue.h"

#include <fstream>
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

bool load_nnue(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    
    if (!file.is_open()) {
        std::cerr << "NNUE file not found: " << filename << std::endl;
        return false;
    }

    file.read(reinterpret_cast<char*>(feature_weights), sizeof(feature_weights));
    file.read(reinterpret_cast<char*>(feature_biases), sizeof(feature_biases));
    file.read(reinterpret_cast<char*>(output_weights), sizeof(output_weights));
    file.read(reinterpret_cast<char*>(&output_bias), sizeof(output_bias));

    file.close();
    std::cout << "NNUE successfully loaded!" << std::endl;
    return true;
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