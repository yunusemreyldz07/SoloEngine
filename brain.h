#ifndef Brain_H
#define Brain_H

#include "board.h"
#include <cmath>
#include <cstdint>
#include <fstream>
#include <random>
#include <string>
#include <vector>

constexpr int INPUT_SIZE = 768;
constexpr int HIDDEN1_SIZE = 128;
constexpr int HIDDEN2_SIZE = 32;

struct TrainingSample {
  std::vector<int> features;
  float target;
};

class Brain {
public:
  float w1[INPUT_SIZE][HIDDEN1_SIZE];
  float b1[HIDDEN1_SIZE];
  float w2[HIDDEN1_SIZE][HIDDEN2_SIZE];
  float b2[HIDDEN2_SIZE];
  float w3[HIDDEN2_SIZE];
  float b3;

  float h1[HIDDEN1_SIZE];
  float h2[HIDDEN2_SIZE];

  float learningRate = 0.001f;
  int gamesLearned = 0;

  Brain();

  void initRandom();
  std::vector<int> extractFeatures(const Board &b) const;
  int evaluate(const Board &b);
  float forward(const std::vector<int> &features);
  void train(const std::vector<TrainingSample> &samples);
  void backward(const std::vector<int> &features, float target, float output);
  bool saveWeights(const std::string &path);
  bool loadWeights(const std::string &path);
};

extern Brain brain;
extern bool useBrain;

#endif
