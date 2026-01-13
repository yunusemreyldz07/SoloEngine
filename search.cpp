#include "search.h"
#include "evaluation.h"
#include "params.h"
#include "sacrifice.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <iostream>
#include <mutex>
#include <random>
#include <thread>

std::atomic<bool> stopSearch(false);
std::atomic<long long> nodeCount(0);
std::atomic<long long> startTime(0);
std::atomic<long long> timeLimit(0);
std::atomic<bool> timeLimited(false);
int uciThreads = 1;

Move killerMoves[MAX_PLY][2];
int historyTable[64][64];
Move counterMoves[64][64];

std::mutex bestMutex;
Move bestMoveGlobal;
int bestDepthGlobal = 0;

void resetNodes() {
  nodeCount.store(0);
  std::memset(killerMoves, 0, sizeof(killerMoves));
  std::memset(historyTable, 0, sizeof(historyTable));
  std::memset(counterMoves, 0, sizeof(counterMoves));
}
long long getNodes() { return nodeCount.load(); }
void requestStop() { stopSearch.store(true); }

static void checkTime() {
  if (!timeLimited.load())
    return;
  auto now = std::chrono::steady_clock::now();
  long long elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                          now.time_since_epoch())
                          .count() -
                      startTime.load();
  if (elapsed >= timeLimit.load())
    stopSearch.store(true);
}

static int see(Board &b, Move &m) {
  static const int vals[] = {0, 100, 320, 330, 500, 900, 20000};
  if (!m.capturedPiece)
    return 0;
  return vals[std::abs(m.capturedPiece)] -
         vals[std::abs(b.squares[m.fromRow * 8 + m.fromCol])] / 10;
}

// Agresif hamle mi? (Düşman kralına yakın hamle veya saldırı)
static bool isAggressiveMove(const Board &b, const Move &m) {
  int enemyKingSq = b.whiteTurn ? b.bKingSq : b.wKingSq;
  int enemyKingFile = enemyKingSq % 8;
  int enemyKingRank = enemyKingSq / 8;

  int toFile = m.toCol;
  int toRank = m.toRow;

  // Hedef kare düşman kralına ne kadar yakın?
  int dist = std::max(std::abs(toFile - enemyKingFile),
                      std::abs(toRank - enemyKingRank));

  // 3 kare içindeyse agresif
  if (dist <= 3)
    return true;

  // Yakalama hamlesi
  if (m.capturedPiece)
    return true;

  // Piyon ilerlemesi (5. sıradan sonra)
  int piece = std::abs(b.squares[m.fromRow * 8 + m.fromCol]);
  if (piece == 1) {
    int relRank = b.whiteTurn ? (7 - m.toRow) : m.toRow;
    if (relRank >= 4)
      return true;
  }

  return false;
}

// Sacrifice hamlesi mi?
static bool isSacrificeMove(Board &b, const Move &m) {
  if (!m.capturedPiece)
    return false;

  int ourPiece = std::abs(b.squares[m.fromRow * 8 + m.fromCol]);
  int theirPiece = std::abs(m.capturedPiece);

  static const int vals[] = {0, 100, 320, 330, 500, 900, 20000};

  // Düşük değerli taşla yüksek değerli taşı almak sacrifice değil
  // Yüksek değerli taşla düşük almak sacrifice olabilir
  if (vals[ourPiece] > vals[theirPiece] + 100) {
    // Bu bir sacrifice - eğer saldırı pozisyonundaysak bonus ver
    int enemyKingSq = b.whiteTurn ? b.bKingSq : b.wKingSq;
    int dist = std::max(std::abs(m.toCol - (enemyKingSq % 8)),
                        std::abs(m.toRow - (enemyKingSq / 8)));
    return dist <= 3;
  }

  return false;
}

static int quiesce(Board &b, int alpha, int beta) {
  nodeCount.fetch_add(1);
  if ((nodeCount.load() & 2047) == 0)
    checkTime();
  if (stopSearch.load())
    return 0;
  int stand = evaluate(b);
  if (stand >= beta)
    return beta;
  if (stand > alpha)
    alpha = stand;
  auto moves = generateMoves(b);
  std::vector<Move> caps;
  for (auto &m : moves)
    if (m.capturedPiece) {
      m.score = see(b, m);
      caps.push_back(m);
    }
  std::sort(caps.begin(), caps.end(),
            [](const Move &a, const Move &b) { return a.score > b.score; });
  for (auto &m : caps) {
    b.makeMove(m);
    int kingSq = !b.whiteTurn ? b.wKingSq : b.bKingSq;
    if (isAttacked(b, kingSq, b.whiteTurn)) {
      b.unmakeMove(m);
      continue;
    }
    int score = -quiesce(b, -beta, -alpha);
    b.unmakeMove(m);
    if (stopSearch.load())
      return 0;
    if (score >= beta)
      return beta;
    if (score > alpha)
      alpha = score;
  }
  return alpha;
}

static int negamax(Board &b, int depth, int alpha, int beta, int ply,
                   std::vector<uint64_t> &hist, std::vector<Move> &pv,
                   bool root) {
  nodeCount.fetch_add(1);
  if ((nodeCount.load() & 2047) == 0)
    checkTime();
  if (stopSearch.load())
    return 0;
  if (!root && (isRepetition(hist) || b.isInsufficientMaterial()))
    return 0;
  if (depth <= 0)
    return quiesce(b, alpha, beta);
  uint64_t key = positionKey(b);
  int ttScore, ttDepth;
  TTFlag ttFlag;
  Move ttMove;
  if (tt.probe(key, ttScore, ttDepth, ttFlag, ttMove) && ttDepth >= depth &&
      !root) {
    if (ttFlag == TTFlag::EXACT)
      return ttScore;
    if (ttFlag == TTFlag::LOWER && ttScore >= beta)
      return beta;
    if (ttFlag == TTFlag::UPPER && ttScore <= alpha)
      return alpha;
  }
  int kingSq = b.whiteTurn ? b.wKingSq : b.bKingSq;
  bool inCheck = isAttacked(b, kingSq, !b.whiteTurn);

  int eval = evaluate(b);

  if (!inCheck && !root && depth <= 3 && eval + 300 < alpha) {
    int razorScore = quiesce(b, alpha, beta);
    if (razorScore < alpha)
      return razorScore;
  }

  if (!inCheck && depth >= 3 && !root && eval >= beta) {
    int R = 2 + depth / 4 + std::min(3, (eval - beta) / 200);
    b.whiteTurn = !b.whiteTurn;
    int nullScore =
        -negamax(b, depth - 1 - R, -beta, -beta + 1, ply + 1, hist, pv, false);
    b.whiteTurn = !b.whiteTurn;
    if (stopSearch.load())
      return 0;
    if (nullScore >= beta)
      return beta;
  }

  if (inCheck)
    depth++;
  auto moves = generateMoves(b);

  for (auto &m : moves) {
    int from = m.fromRow * 8 + m.fromCol;
    int to = m.toRow * 8 + m.toCol;

    if (m.fromRow == ttMove.fromRow && m.fromCol == ttMove.fromCol &&
        m.toRow == ttMove.toRow && m.toCol == ttMove.toCol) {
      m.score = 10000000;
    } else if (m.capturedPiece) {
      int seeScore = see(b, m);
      if (seeScore >= 0)
        m.score = 9000000 + seeScore;
      else {
        // Sacrifice hamleleri için özel değerlendirme
        if (isSacrificeMove(b, m)) {
          m.score = 8500000 + seeScore; // Sacrifice bonusu
        } else {
          m.score = -1000000 + seeScore;
        }
      }
    } else {
      bool isKiller1 =
          (ply < MAX_PLY && m.fromRow == killerMoves[ply][0].fromRow &&
           m.fromCol == killerMoves[ply][0].fromCol &&
           m.toRow == killerMoves[ply][0].toRow &&
           m.toCol == killerMoves[ply][0].toCol);
      bool isKiller2 =
          (ply < MAX_PLY && m.fromRow == killerMoves[ply][1].fromRow &&
           m.fromCol == killerMoves[ply][1].fromCol &&
           m.toRow == killerMoves[ply][1].toRow &&
           m.toCol == killerMoves[ply][1].toCol);

      bool isCounter = false;
      if (ply > 0) {
        isCounter = (m.fromRow == counterMoves[from][to].fromRow &&
                     m.fromCol == counterMoves[from][to].fromCol &&
                     m.toRow == counterMoves[from][to].toRow &&
                     m.toCol == counterMoves[from][to].toCol);
      }

      if (isKiller1)
        m.score = 8000000;
      else if (isKiller2)
        m.score = 7900000;
      else if (isCounter)
        m.score = 7000000;
      else {
        m.score = historyTable[from][to];
        // Agresif hamlelere bonus
        if (isAggressiveMove(b, m)) {
          m.score += 500000; // Agresif bonus
        }
      }
    }
  }
  std::sort(moves.begin(), moves.end(),
            [](const Move &a, const Move &b) { return a.score > b.score; });
  TTFlag flag = TTFlag::UPPER;
  int bestScore = -2000000000;
  Move bestMove;
  std::vector<Move> childPV;
  int legal = 0;

  bool futilityPruning = false;
  int futilityMargin = 0;
  if (!inCheck && depth <= 6 && eval + 50 * depth < alpha) {
    futilityPruning = true;
    futilityMargin = 50 + 50 * depth;
  }

  for (auto &m : moves) {
    if (futilityPruning && !m.capturedPiece && !m.promotion && legal > 0 &&
        eval + futilityMargin < alpha) {
      continue;
    }

    b.makeMove(m);
    int testKing = !b.whiteTurn ? b.wKingSq : b.bKingSq;
    if (isAttacked(b, testKing, b.whiteTurn)) {
      b.unmakeMove(m);
      continue;
    }
    legal++;
    hist.push_back(positionKey(b));
    childPV.clear();

    int score;
    bool needFullSearch = true;

    if (legal > 3 && depth >= 3 && !m.capturedPiece && !m.promotion &&
        !inCheck) {
      int reduction = 1;
      if (legal > 6) {
        reduction = 1 + (int)(0.5 + std::log((double)depth) *
                                        std::log((double)legal) / 2.0);
        reduction = std::min(depth - 1, reduction);
      }

      // Agresif hamleler için azaltılmış LMR
      if (isAggressiveMove(b, m)) {
        reduction = std::max(0, reduction - 1);
      }

      score = -negamax(b, depth - 1 - reduction, -alpha - 1, -alpha, ply + 1,
                       hist, childPV, false);
      if (score > alpha && reduction > 0) {
        needFullSearch = true;
      } else {
        needFullSearch = false;
      }
    }

    if (needFullSearch) {
      if (legal == 1) {
        score = -negamax(b, depth - 1, -beta, -alpha, ply + 1, hist, childPV,
                         false);
      } else {
        score = -negamax(b, depth - 1, -alpha - 1, -alpha, ply + 1, hist,
                         childPV, false);
        if (score > alpha && score < beta) {
          score = -negamax(b, depth - 1, -beta, -alpha, ply + 1, hist, childPV,
                           false);
        }
      }
    }

    hist.pop_back();
    b.unmakeMove(m);
    if (stopSearch.load())
      return 0;
    if (score > bestScore) {
      bestScore = score;
      bestMove = m;
      if (score > alpha) {
        alpha = score;
        pv.clear();
        pv.push_back(m);
        pv.insert(pv.end(), childPV.begin(), childPV.end());
        flag = TTFlag::EXACT;
      }
    }
    if (alpha >= beta) {
      flag = TTFlag::LOWER;
      if (!bestMove.capturedPiece && ply < MAX_PLY) {
        int from = bestMove.fromRow * 8 + bestMove.fromCol;
        int to = bestMove.toRow * 8 + bestMove.toCol;

        if (killerMoves[ply][0].fromRow != bestMove.fromRow ||
            killerMoves[ply][0].fromCol != bestMove.fromCol ||
            killerMoves[ply][0].toRow != bestMove.toRow ||
            killerMoves[ply][0].toCol != bestMove.toCol) {
          killerMoves[ply][1] = killerMoves[ply][0];
          killerMoves[ply][0] = bestMove;
        }

        historyTable[from][to] += depth * depth;
        if (historyTable[from][to] > 1000000)
          historyTable[from][to] = 1000000;

        if (ply > 0) {
          counterMoves[from][to] = bestMove;
        }
      }
      break;
    }
  }
  if (legal == 0)
    return inCheck ? -30000 + ply : 0;
  tt.store(key, bestScore, depth, flag, bestMove);
  return bestScore;
}

static void searchThread(Board b, int maxDepth, std::vector<uint64_t> hist,
                         bool main) {
  std::vector<Move> pv;
  Move currentBest;
  int prevScore = 0;

  for (int d = 1; d <= maxDepth; d++) {
    pv.clear();
    int score;

    if (d >= 5 && !stopSearch.load()) {
      int delta = 25;
      int alpha = prevScore - delta;
      int beta = prevScore + delta;

      score = negamax(b, d, alpha, beta, 0, hist, pv, true);

      if ((score <= alpha || score >= beta) && !stopSearch.load()) {
        score = negamax(b, d, -2000000000, 2000000000, 0, hist, pv, true);
      }
    } else {
      score = negamax(b, d, -2000000000, 2000000000, 0, hist, pv, true);
    }

    prevScore = score;
    if (stopSearch.load())
      break;
    if (!pv.empty())
      currentBest = pv[0];
    if (main) {
      std::lock_guard<std::mutex> lock(bestMutex);
      if (!pv.empty() && d > bestDepthGlobal) {
        bestMoveGlobal = pv[0];
        bestDepthGlobal = d;
      } else if (d == 1 && bestDepthGlobal == 0 && currentBest.fromRow != 0) {
        bestMoveGlobal = currentBest;
        bestDepthGlobal = 1;
      }

      if (params.randomness > 0 && d <= 3) {
        static std::mt19937 rng(
            std::chrono::steady_clock::now().time_since_epoch().count());
        std::uniform_int_distribution<int> dist(-params.randomness * 10,
                                                params.randomness * 10);
        score += dist(rng);
      }

      long long nodes = nodeCount.load();
      long long elapsed =
          std::chrono::duration_cast<std::chrono::milliseconds>(
              std::chrono::steady_clock::now().time_since_epoch())
              .count() -
          startTime.load();
      std::cout << "info depth " << d << " score cp " << score << " nodes "
                << nodes << " nps "
                << (elapsed > 0 ? nodes * 1000 / elapsed : 0) << " time "
                << elapsed << " pv";
      for (const auto &m : pv)
        std::cout << " " << moveToUCI(m);
      std::cout << std::endl;
    }
  }
}

Move search(Board &b, int maxDepth, int timeMs,
            const std::vector<uint64_t> &history) {
  resetNodes();
  stopSearch.store(false);
  bestMoveGlobal = Move();
  bestDepthGlobal = 0;
  auto now = std::chrono::steady_clock::now();
  startTime.store(std::chrono::duration_cast<std::chrono::milliseconds>(
                      now.time_since_epoch())
                      .count());
  if (timeMs > 0) {
    timeLimited.store(true);
    timeLimit.store(timeMs > 50 ? timeMs - 20 : timeMs);
  } else {
    timeLimited.store(false);
  }
  std::vector<std::thread> threads;
  int numThreads = std::max(1, uciThreads);
  for (int i = 1; i < numThreads; i++)
    threads.emplace_back(searchThread, b, maxDepth + 4, history, false);
  searchThread(b, maxDepth, history, true);
  requestStop();
  for (auto &t : threads)
    if (t.joinable())
      t.join();
  std::lock_guard<std::mutex> lock(bestMutex);
  return bestMoveGlobal;
}
