#include "nnue.h"
#include <cstring>
#include "nnue_data.h"
#include <iostream>
#include <array>
#include "board.h"
#include <algorithm>

constexpr int INPUT_SIZE = 768;
constexpr int HIDDEN_SIZE = 128;


using Accumulator = std::array<int16_t, HIDDEN_SIZE>;


// NNUE Weights and Biases
alignas(64) int16_t hiddenWeight[INPUT_SIZE * HIDDEN_SIZE]; 
alignas(64) int16_t hiddenBias[HIDDEN_SIZE];

alignas(64) int16_t outputWeight[HIDDEN_SIZE * 2]; 
int16_t outputBias;

int32_t screlu(int16_t x) {
    int32_t y = x;
    y = std::clamp(y, 0, 255);
    return y * y;
}

void updateAccumulator(Accumulator& acc, int featureIdx, bool isAdd) { 
    int offset = featureIdx * HIDDEN_SIZE;
    
    if (isAdd) {
        for (int i = 0; i < HIDDEN_SIZE; i++) {
            acc[i] += hiddenWeight[offset + i];
        }
    } else {
        for (int i = 0; i < HIDDEN_SIZE; i++) {
            acc[i] -= hiddenWeight[offset + i];
        }
    }

    /*
    hiddenWeight is actually a 2D array. 
    But we store it as a 1D array for cache efficiency. 
    The index in the 1D array is calculated as featureIdx * HIDDEN_SIZE + i, where featureIdx is the index of the input feature (0 to 767) and i is the index of the hidden layer neuron (0 to 1023). 
    This way, we can efficiently access the weights for each feature and hidden neuron without needing to do complex pointer arithmetic or multi-dimensional indexing.
    */
}

int makeFeatureIndex(int piece_type, int piece_color, int square, int perspective) {
    // (if perspective is 0 it remains the same, if perspective is 1 it flips to the opposite color and square)
    int is_enemy = piece_color ^ perspective; 
    
    int pieceIdx = (piece_type - 1) + (is_enemy * 6);
    
    // if the perspective is zero (white), the square index remains the same. If the perspective is one (black), we flip the square index to mirror it vertically.
    int sq = square ^ (perspective * 56);

    return (pieceIdx * 64) + sq;
}

void load_nnue() {
    size_t offset = 0;

    std::memcpy(hiddenWeight, nnue_data + offset, sizeof(hiddenWeight));
    offset += sizeof(hiddenWeight);

    std::memcpy(hiddenBias, nnue_data + offset, sizeof(hiddenBias));
    offset += sizeof(hiddenBias);

    std::memcpy(outputWeight, nnue_data + offset, sizeof(outputWeight));
    offset += sizeof(outputWeight);

    std::memcpy(&outputBias, nnue_data + offset, sizeof(outputBias));
}

void RefreshAccumulator(const Board& board, Accumulator* acc_white, Accumulator* acc_black) {
    for (int i = 0; i < HIDDEN_SIZE; i++) {
        (*acc_white)[i] = hiddenBias[i];
        (*acc_black)[i] = hiddenBias[i];
    }

    for (int sq = 0; sq < 64; sq++) {
        int piece = board.mailbox[sq];
        
        if (piece != 0) {
            int p_color = (piece - 1) / 6; 
            int p_type  = ((piece - 1) % 6) + 1; 

            int white_idx = makeFeatureIndex(p_type, p_color, sq, WHITE); // WHITE = 0
            updateAccumulator(*acc_white, white_idx, true);

            int black_idx = makeFeatureIndex(p_type, p_color, sq, BLACK); // BLACK = 1
            updateAccumulator(*acc_black, black_idx, true);
        }
    }
}

int evaluate_nnue(const Accumulator& acc_white, const Accumulator& acc_black, int side_to_move) {
    const Accumulator& us = (side_to_move == WHITE) ? acc_white : acc_black;
    const Accumulator& them = (side_to_move == WHITE) ? acc_black : acc_white;

    int32_t raw_sum = 0; 

    for (int i = 0; i < HIDDEN_SIZE; i++) {
        raw_sum += screlu(us[i]) * outputWeight[i];
        raw_sum += screlu(them[i]) * outputWeight[HIDDEN_SIZE + i];
    }

    // Turning it into a real chess score (QA=255, QB=64, Scale=400)
    int finalScore = ((raw_sum / 255) + outputBias) * 400 / 16320; // 255 * 64 = 16320
    
    return finalScore;
}