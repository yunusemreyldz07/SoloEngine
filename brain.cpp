#include "brain.h"
#include <algorithm>
#include <cstring>
#include <iostream>

Brain brain;
bool useBrain = false;

Brain::Brain() { initRandom(); }

void Brain::initRandom() {
  std::mt19937 rng(42);
  std::normal_distribution<float> dist(0.0f, 0.1f);

  for (int i = 0; i < INPUT_SIZE; i++) {
    for (int j = 0; j < HIDDEN1_SIZE; j++) {
      w1[i][j] = dist(rng);
    }
  }
  for (int i = 0; i < HIDDEN1_SIZE; i++) {
    b1[i] = 0.0f;
    for (int j = 0; j < HIDDEN2_SIZE; j++) {
      w2[i][j] = dist(rng);
    }
  }
  for (int i = 0; i < HIDDEN2_SIZE; i++) {
    b2[i] = 0.0f;
    w3[i] = dist(rng);
  }
  b3 = 0.0f;
  gamesLearned = 0;
}

std::vector<int> Brain::extractFeatures(const Board &b) const {
  std::vector<int> features;
  for (int sq = 0; sq < 64; sq++) {
    int piece = b.squares[sq];
    if (piece == 0)
      continue;
    int side = piece > 0 ? 0 : 1;
    int pt = std::abs(piece) - 1;
    int idx = side * 384 + pt * 64 + sq;
    features.push_back(idx);
  }
  return features;
}

float Brain::forward(const std::vector<int> &features) {
  std::memset(h1, 0, sizeof(h1));
  for (int f : features) {
    for (int j = 0; j < HIDDEN1_SIZE; j++) {
      h1[j] += w1[f][j];
    }
  }
  for (int j = 0; j < HIDDEN1_SIZE; j++) {
    h1[j] = std::max(0.0f, h1[j] + b1[j]);
  }

  std::memset(h2, 0, sizeof(h2));
  for (int i = 0; i < HIDDEN1_SIZE; i++) {
    for (int j = 0; j < HIDDEN2_SIZE; j++) {
      h2[j] += h1[i] * w2[i][j];
    }
  }
  for (int j = 0; j < HIDDEN2_SIZE; j++) {
    h2[j] = std::max(0.0f, h2[j] + b2[j]);
  }

  float out = b3;
  for (int i = 0; i < HIDDEN2_SIZE; i++) {
    out += h2[i] * w3[i];
  }

  return std::tanh(out);
}

int Brain::evaluate(const Board &b) {
  auto features = extractFeatures(b);
  float score = forward(features);
  int cp = (int)(score * 600.0f);
  return b.whiteTurn ? cp : -cp;
}

void Brain::backward(const std::vector<int> &features, float target,
                    float output) {
  float dOut = 2.0f * (output - target) * (1.0f - output * output);

  float dH2[HIDDEN2_SIZE];
  for (int i = 0; i < HIDDEN2_SIZE; i++) {
    dH2[i] = dOut * w3[i] * (h2[i] > 0 ? 1.0f : 0.0f);
    w3[i] -= learningRate * dOut * h2[i];
  }
  b3 -= learningRate * dOut;

  float dH1[HIDDEN1_SIZE];
  std::memset(dH1, 0, sizeof(dH1));
  for (int i = 0; i < HIDDEN1_SIZE; i++) {
    for (int j = 0; j < HIDDEN2_SIZE; j++) {
      dH1[i] += dH2[j] * w2[i][j];
      w2[i][j] -= learningRate * dH2[j] * h1[i];
    }
    dH1[i] *= (h1[i] > 0 ? 1.0f : 0.0f);
  }
  for (int j = 0; j < HIDDEN2_SIZE; j++) {
    b2[j] -= learningRate * dH2[j];
  }

  for (int f : features) {
    for (int j = 0; j < HIDDEN1_SIZE; j++) {
      w1[f][j] -= learningRate * dH1[j];
    }
  }
  for (int j = 0; j < HIDDEN1_SIZE; j++) {
    b1[j] -= learningRate * dH1[j];
  }
}

void Brain::train(const std::vector<TrainingSample> &samples) {
  for (const auto &s : samples) {
    float output = forward(s.features);
    backward(s.features, s.target, output);
  }
  gamesLearned++;
}

bool Brain::saveWeights(const std::string &path) {
  std::ofstream f(path, std::ios::binary);
  if (!f)
    return false;

  char magic[4] = {'S', 'O', 'L', 'O'};
  f.write(magic, 4);
  f.write(reinterpret_cast<char *>(&gamesLearned), sizeof(int));
  f.write(reinterpret_cast<char *>(w1), sizeof(w1));
  f.write(reinterpret_cast<char *>(b1), sizeof(b1));
  f.write(reinterpret_cast<char *>(w2), sizeof(w2));
  f.write(reinterpret_cast<char *>(b2), sizeof(b2));
  f.write(reinterpret_cast<char *>(w3), sizeof(w3));
  f.write(reinterpret_cast<char *>(&b3), sizeof(float));

  f.close();
  return true;
}

bool Brain::loadWeights(const std::string &path) {
  std::ifstream f(path, std::ios::binary);
  if (!f)
    return false;

  char magic[4];
  f.read(magic, 4);
  if (magic[0] != 'S' || magic[1] != 'O' || magic[2] != 'L' ||
      magic[3] != 'O') {
    return false;
  }

  f.read(reinterpret_cast<char *>(&gamesLearned), sizeof(int));
  f.read(reinterpret_cast<char *>(w1), sizeof(w1));
  f.read(reinterpret_cast<char *>(b1), sizeof(b1));
  f.read(reinterpret_cast<char *>(w2), sizeof(w2));
  f.read(reinterpret_cast<char *>(b2), sizeof(b2));
  f.read(reinterpret_cast<char *>(w3), sizeof(w3));
  f.read(reinterpret_cast<char *>(&b3), sizeof(float));

  f.close();
  return true;
}
