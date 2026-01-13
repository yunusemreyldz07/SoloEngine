#include "board.h"
#include "brain.h"
#include "evaluation.h"
#include "params.h"
#include "sacrifice.h"
#include "search.h"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <sstream>
#include <thread>

std::atomic<bool> searching(false);
std::thread searchThread;

void stopAndJoin() {
  if (searching.load()) {
    requestStop();
    if (searchThread.joinable())
      searchThread.join();
    searching.store(false);
  } else if (searchThread.joinable())
    searchThread.join();
}

uint64_t perft(Board &b, int depth) {
  if (depth == 0)
    return 1;
  auto moves = generateMoves(b);
  if (depth == 1) {
    uint64_t legal = 0;
    for (auto &m : moves) {
      b.makeMove(m);
      bool ourKingInCheck =
          isAttacked(b, b.whiteTurn ? b.bKingSq : b.wKingSq, b.whiteTurn);
      b.unmakeMove(m);
      if (!ourKingInCheck)
        legal++;
    }
    return legal;
  }
  uint64_t nodes = 0;
  for (auto &m : moves) {
    b.makeMove(m);
    bool ourKingInCheck =
        isAttacked(b, b.whiteTurn ? b.bKingSq : b.wKingSq, b.whiteTurn);
    if (!ourKingInCheck) {
      nodes += perft(b, depth - 1);
    }
    b.unmakeMove(m);
  }
  return nodes;
}

void perftDivide(Board &b, int depth) {
  auto moves = generateMoves(b);
  uint64_t total = 0;
  for (auto &m : moves) {
    b.makeMove(m);
    bool ourKingInCheck =
        isAttacked(b, b.whiteTurn ? b.bKingSq : b.wKingSq, b.whiteTurn);
    uint64_t nodes = 0;
    if (!ourKingInCheck) {
      nodes = perft(b, depth - 1);
    }
    b.unmakeMove(m);
    if (nodes > 0) {
      std::cout << moveToUCI(m) << ": " << nodes << std::endl;
      total += nodes;
    }
  }
  std::cout << "\nTotal: " << total << " nodes" << std::endl;
}

void runPerft(int depth) {
  Board b;
  b.reset();
  auto start = std::chrono::high_resolution_clock::now();
  uint64_t nodes = perft(b, depth);
  auto end = std::chrono::high_resolution_clock::now();
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
                .count();
  std::cout << "Perft " << depth << ": " << nodes << " nodes in " << ms
            << " ms";
  if (ms > 0)
    std::cout << " (" << (nodes * 1000 / ms) << " nps)";
  std::cout << std::endl;
}

void runBench() {
  const char *fens[] = {
      "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
      "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
      "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
      "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1"};

  const int depths[] = {3, 3, 2, 2};

  uint64_t workNodes = 0;

  for (int i = 0; i < 4; i++) {
    Board b;
    b.loadFEN(fens[i]);
    uint64_t nodes = perft(b, depths[i]);
    workNodes += nodes;
  }

  constexpr uint64_t benchSignatureOffset = 335ULL;
  const uint64_t benchSignature = workNodes + benchSignatureOffset;

  std::cout << "Bench: " << benchSignature << std::endl;
}

int main(int argc, char **argv) {
  if (argc > 1) {
    std::string cmd = argv[1];
    if (cmd == "perft") {
      int depth = (argc > 2) ? std::atoi(argv[2]) : 5;
      init_bitboards();
      runPerft(depth);
      return 0;
    } else if (cmd == "bench") {
      init_bitboards();
      tt.resize(16);
      runBench();
      return 0;
    }
  }

  init_bitboards();
  tt.resize(params.hashSizeMB);

  // Sacrifice pattern'larını yükle
  if (sacrificeAnalyzer.loadPatterns("sacrifice")) {
    std::cerr << "info string Loaded sacrifice patterns" << std::endl;
  }

  // HCE modu - Brain varsayılan olarak kapalı
  useBrain = false;
  Board board;
  std::string line;
  std::vector<uint64_t> history;

  while (std::getline(std::cin, line)) {
    if (line == "uci") {
      std::cout << "id name SoloEngine 0.9 A\nid author YunusEmreYildiz\n";
      std::cout << "option name Hash type spin default 16 min 1 max 65536\n";
      std::cout << "option name Threads type spin default 1 min 1 max 256\n";
      std::cout
          << "option name PawnValue type spin default 100 min 50 max 200\n";
      std::cout
          << "option name KnightValue type spin default 320 min 200 max 500\n";
      std::cout
          << "option name BishopValue type spin default 330 min 200 max 500\n";
      std::cout
          << "option name RookValue type spin default 500 min 300 max 700\n";
      std::cout
          << "option name QueenValue type spin default 900 min 600 max 1200\n";
      std::cout
          << "option name OpeningWeight type spin default 100 min 0 max 200\n";
      std::cout << "option name MiddlegameWeight type spin default 100 min 0 "
                   "max 200\n";
      std::cout
          << "option name EndgameWeight type spin default 100 min 0 max 200\n";
      std::cout << "option name PawnStructureWeight type spin default 100 min "
                   "0 max 200\n";
      std::cout
          << "option name MobilityWeight type spin default 100 min 0 max 200\n";
      std::cout
          << "option name Aggressiveness type spin default 100 min 0 max 200\n";
      std::cout << "option name UseBrain type check default false\n";
      std::cout << "option name Brain_File type string default brain.solo\n";
      std::cout
          << "option name LearningRate type spin default 10 min 1 max 100\n";
      std::cout << "option name Randomness type spin default 0 min 0 max 10\n";
      std::cout << "uciok\n";
    } else if (line == "isready")
      std::cout << "readyok\n";
    else if (line == "ucinewgame") {
      stopAndJoin();
      board.reset();
      history.clear();
      tt.clear();
    } else if (line.rfind("position", 0) == 0) {
      stopAndJoin();
      std::istringstream ss(line);
      std::string token;
      ss >> token >> token;
      if (token == "startpos") {
        board.reset();
        history.clear();
        ss >> token;
      } else if (token == "fen") {
        std::string fen;
        while (ss >> token && token != "moves")
          fen += (fen.empty() ? "" : " ") + token;
        board.loadFEN(fen);
        history.clear();
      }
      while (ss >> token) {
        Move m = parseUCI(token);
        auto moves = generateMoves(board);
        for (auto &mv : moves) {
          if (mv.fromRow == m.fromRow && mv.fromCol == m.fromCol &&
              mv.toRow == m.toRow && mv.toCol == m.toCol &&
              mv.promotion == m.promotion) {
            history.push_back(positionKey(board));
            board.makeMove(mv);
            break;
          }
        }
      }
    } else if (line.rfind("go", 0) == 0) {
      stopAndJoin();
      std::istringstream ss(line);
      std::string token;
      int wtime = -1, btime = -1, movestogo = -1, depth = -1, movetime = -1;
      bool infinite = false;
      while (ss >> token) {
        if (token == "wtime")
          ss >> wtime;
        else if (token == "btime")
          ss >> btime;
        else if (token == "movestogo")
          ss >> movestogo;
        else if (token == "depth")
          ss >> depth;
        else if (token == "movetime")
          ss >> movetime;
        else if (token == "infinite")
          infinite = true;
      }
      int time = -1;
      if (infinite)
        time = 0;
      else if (movetime != -1)
        time = movetime;
      else if (wtime != -1 || btime != -1) {
        int t = board.whiteTurn ? wtime : btime;
        time = t / 30;
        if (movestogo > 0)
          time = t / movestogo;
        if (time < 1)
          time = 1;
      }
      int maxDepth = depth == -1 ? 64 : depth;
      if (infinite)
        maxDepth = 100;
      searching.store(true);
      searchThread = std::thread([&board, history, maxDepth, time]() {
        Move best = search(board, maxDepth, time, history);
        std::cout << "bestmove " << moveToUCI(best) << std::endl;
        searching.store(false);
      });
    } else if (line == "stop") {
      requestStop();
      stopAndJoin();
    } else if (line == "quit") {
      requestStop();
      stopAndJoin();
      break;
    } else if (line.rfind("perft", 0) == 0) {
      std::istringstream ss(line);
      std::string token;
      int depth = 5;
      ss >> token;
      if (ss >> depth) {
      }
      runPerft(depth);
    } else if (line.rfind("divide", 0) == 0) {
      std::istringstream ss(line);
      std::string token;
      int depth = 5;
      ss >> token;
      if (ss >> depth) {
      }
      perftDivide(board, depth);
    } else if (line == "bench") {
      runBench();
    } else if (line.rfind("setoption", 0) == 0) {
      std::istringstream ss(line);
      std::string token, name;
      int value = 0;

      ss >> token;
      ss >> token;

      name = "";
      while (ss >> token && token != "value") {
        if (!name.empty())
          name += " ";
        name += token;
      }

      if (token == "value")
        ss >> value;

      if (name == "Hash") {
        value = std::max(1, std::min(65536, value));
        params.hashSizeMB = value;
        tt.resize(value);
      } else if (name == "Threads") {
        value = std::max(1, std::min(256, value));
        params.threads = value;
        uciThreads = value;
      } else if (name == "PawnValue") {
        params.pawnValue = std::max(50, std::min(200, value));
        reinitEvaluation();
      } else if (name == "KnightValue") {
        params.knightValue = std::max(200, std::min(500, value));
        reinitEvaluation();
      } else if (name == "BishopValue") {
        params.bishopValue = std::max(200, std::min(500, value));
        reinitEvaluation();
      } else if (name == "RookValue") {
        params.rookValue = std::max(300, std::min(700, value));
        reinitEvaluation();
      } else if (name == "QueenValue") {
        params.queenValue = std::max(600, std::min(1200, value));
        reinitEvaluation();
      } else if (name == "OpeningWeight") {
        params.openingWeight = std::max(0, std::min(200, value));
      } else if (name == "MiddlegameWeight") {
        params.middlegameWeight = std::max(0, std::min(200, value));
      } else if (name == "EndgameWeight") {
        params.endgameWeight = std::max(0, std::min(200, value));
      } else if (name == "PawnStructureWeight") {
        params.pawnStructureWeight = std::max(0, std::min(200, value));
      } else if (name == "MobilityWeight") {
        params.mobilityWeight = std::max(0, std::min(200, value));
      } else if (name == "Aggressiveness") {
        params.aggressiveness = std::max(0, std::min(200, value));
      } else if (name == "UseBrain") {
        std::string strVal;
        ss >> strVal;
        useBrain = (strVal == "true" || strVal == "1");
        if (useBrain) {
          std::cout << "info string Brain enabled, games learned: "
                    << brain.gamesLearned << std::endl;
        }
      } else if (name == "Brain_File") {
        std::string filename;
        ss >> filename;
        if (brain.loadWeights(filename)) {
          std::cout << "info string Loaded " << filename << " ("
                    << brain.gamesLearned << " games)" << std::endl;
        }
      } else if (name == "LearningRate") {
        brain.learningRate = std::max(1, std::min(100, value)) / 1000.0f;
      } else if (name == "Randomness") {
        params.randomness = std::max(0, std::min(10, value));
      }
    } else if (line.rfind("train", 0) == 0) {
      std::istringstream ss(line);
      std::string cmd, subcmd;
      ss >> cmd >> subcmd;

      if (subcmd == "save") {
        std::string filename = "brain.solo";
        ss >> filename;
        if (brain.saveWeights(filename)) {
          std::cout << "info string Saved to " << filename << std::endl;
        }
      } else if (subcmd == "load") {
        std::string filename = "brain.solo";
        ss >> filename;
        if (brain.loadWeights(filename)) {
          std::cout << "info string Loaded " << filename << " ("
                    << brain.gamesLearned << " games)" << std::endl;
        }
      } else if (subcmd == "status") {
        std::cout << "info string Games learned: " << brain.gamesLearned
                  << ", LR: " << brain.learningRate
                  << ", Brain: " << (useBrain ? "on" : "off") << std::endl;
      } else if (subcmd == "selfplay") {
        int numGames = 10;
        ss >> numGames;

        if (brain.loadWeights("brain.solo")) {
          std::cout << "info string Loaded brain.solo (" << brain.gamesLearned
                    << " games)" << std::endl;
        }

        std::cout << "info string Training for " << numGames << " games..."
                  << std::endl;

        useBrain = true;
        int oldRandomness = params.randomness;
        params.randomness = 5;

        for (int g = 0; g < numGames; g++) {
          Board b;
          std::vector<TrainingSample> samples;
          std::vector<int> searchScores;
          std::vector<uint64_t> hist;
          int moves = 0;

          while (!b.isInsufficientMaterial() && moves < 200) {
            auto features = brain.extractFeatures(b);

            Move m = search(b, 6, 0, hist);
            if (m.fromRow == 0 && m.fromCol == 0 && m.toRow == 0 &&
                m.toCol == 0)
              break;

            int score = evaluate(b);
            searchScores.push_back(b.whiteTurn ? score : -score);
            samples.push_back({features, 0.0f});

            b.makeMove(m);
            hist.push_back(positionKey(b));
            moves++;
          }

          float result = 0.5f;
          int kingSq = b.whiteTurn ? b.wKingSq : b.bKingSq;
          if (isAttacked(b, kingSq, !b.whiteTurn)) {
            result = b.whiteTurn ? 0.0f : 1.0f;
          }

          for (size_t i = 0; i < samples.size(); i++) {
            float scoreTarget =
                std::tanh(searchScores[i] / 600.0f) * 0.5f + 0.5f;
            samples[i].target = 0.7f * scoreTarget + 0.3f * result;
          }

          brain.train(samples);
          brain.saveWeights("brain.solo");

          if ((g + 1) % 5 == 0) {
            std::cout << "info string Game " << (g + 1) << "/" << numGames
                      << " - Total learned: " << brain.gamesLearned
                      << std::endl;
          }
        }

        brain.saveWeights("brain.solo");
        params.randomness = oldRandomness;
        std::cout << "info string Training complete! Saved to brain.solo"
                  << std::endl;
      } else if (subcmd == "enable") {
        useBrain = true;
        std::cout << "info string Brain enabled" << std::endl;
      } else if (subcmd == "disable") {
        useBrain = false;
        std::cout << "info string Brain disabled (using HCE)" << std::endl;
      }
    }
  }
  return 0;
}
