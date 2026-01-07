#include "search.h"
#include "board.h"
#include "evaluation.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <iostream>
#include <limits>
#include <vector>

int MATE_SCORE = 100000;

int historyTable[64][64];

const int PIECE_VALUES[7] = {0, 100, 320, 330, 500, 900, 20000};

Move killerMove[2][100];
std::atomic<long long> nodeCount{0};

void resetNodeCounter() { nodeCount.store(0, std::memory_order_relaxed); }

long long getNodeCounter() { return nodeCount.load(std::memory_order_relaxed); }

std::atomic<bool> stop_search(false);
std::atomic<long long> time_limit_ms{0};
std::atomic<long long> start_time_ms{0};
std::atomic<bool> is_time_limited{false};

void request_stop_search() {
  stop_search.store(true, std::memory_order_relaxed);
}

bool should_stop() {
  if (stop_search.load(std::memory_order_relaxed))
    return true;

  if (is_time_limited.load(std::memory_order_relaxed)) {
    if ((nodeCount.load(std::memory_order_relaxed) & 2047) == 0) {
      auto now = std::chrono::steady_clock::now();
      long long now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                             now.time_since_epoch())
                             .count();
      long long elapsed =
          now_ms - start_time_ms.load(std::memory_order_relaxed);
      if (elapsed >= time_limit_ms.load(std::memory_order_relaxed)) {
        stop_search.store(true, std::memory_order_relaxed);
        return true;
      }
    }
  }
  return false;
}

static std::string move_to_uci(const Move &m) {
  std::string s;
  s += columns[m.fromCol];
  s += static_cast<char>('0' + (8 - m.fromRow));
  s += columns[m.toCol];
  s += static_cast<char>('0' + (8 - m.toRow));
  if (m.promotion != 0) {
    switch (std::abs(m.promotion)) {
    case queen:
      s += 'q';
      break;
    case rook:
      s += 'r';
      break;
    case bishop:
      s += 'b';
      break;
    case knight:
      s += 'n';
      break;
    default:
      break;
    }
  }
  return s;
}

int scoreMove(const Board &board, const Move &move, int ply,
              const Move *ttMove) {
  int moveScore = 0;
  int from = move.fromRow * 8 + move.fromCol;
  int to = move.toRow * 8 + move.toCol;
  if (move.capturedPiece != 0 || move.isEnPassant) {
    int victimPiece = move.isEnPassant ? pawn : std::abs(move.capturedPiece);
    int victimValue = PIECE_VALUES[victimPiece];

    int attackerPiece = board.squares[move.fromRow][move.fromCol];
    int attackerValue = PIECE_VALUES[std::abs(attackerPiece)];
    moveScore += 10000 + (victimValue * 10) - attackerValue;
  }

  if (ttMove != nullptr && move.fromRow == ttMove->fromRow &&
      move.fromCol == ttMove->fromCol && move.toRow == ttMove->toRow &&
      move.toCol == ttMove->toCol) {
    return 1000000;
  }

  if (ply >= 0 && ply < 100) {
    if (move.fromCol == killerMove[0][ply].fromCol &&
        move.fromRow == killerMove[0][ply].fromRow &&
        move.toCol == killerMove[0][ply].toCol &&
        move.toRow == killerMove[0][ply].toRow) {
      moveScore += 8000;
    }

    if (move.fromCol == killerMove[1][ply].fromCol &&
        move.fromRow == killerMove[1][ply].fromRow &&
        move.toCol == killerMove[1][ply].toCol &&
        move.toRow == killerMove[1][ply].toRow) {
      moveScore += 7000;
    }
  }

  if (move.promotion != 0) {
    moveScore += 9000;
  }

  if (ply == 0) {
    if (move.fromRow == 6 && move.toRow == 4 &&
        (move.fromCol == 3 || move.fromCol == 4)) {
      moveScore += 500;
    }
    int piece = board.squares[move.fromRow][move.fromCol];
    if (std::abs(piece) == knight) {
      if ((move.fromRow == 7 && move.fromCol == 1 && move.toRow == 5 &&
           move.toCol == 2) ||
          (move.fromRow == 7 && move.fromCol == 6 && move.toRow == 5 &&
           move.toCol == 5) ||
          (move.fromRow == 0 && move.fromCol == 1 && move.toRow == 2 &&
           move.toCol == 2) ||
          (move.fromRow == 0 && move.fromCol == 6 && move.toRow == 2 &&
           move.toCol == 5)) {
        moveScore += 400;
      }
    }
  }

  if (move.isCastling) {
    moveScore += 500;
  }

  if (historyTable[from][to] != 0) {
    moveScore += historyTable[from][to];
  }

  return moveScore;
}

int quiescence(Board &board, int alpha, int beta, int ply) {
  nodeCount.fetch_add(1, std::memory_order_relaxed);
  if (should_stop()) {
    return 0;
  }

  if (ply >= 99) {
    return evaluate_board(board);
  }

  int stand_pat = evaluate_board(board);

  if (stand_pat >= beta) {
    return beta;
  }
  if (alpha < stand_pat) {
    alpha = stand_pat;
  }

  std::vector<Move> captureMoves = get_capture_moves(board);

  std::sort(captureMoves.begin(), captureMoves.end(),
            [&](const Move &a, const Move &b) {
              return scoreMove(board, a, ply, nullptr) >
                     scoreMove(board, b, ply, nullptr);
            });

  for (Move &move : captureMoves) {
    int seeValue = see_exchange(board, move);
    if (seeValue < 0) {
      continue;
    }

    int capturedValue = PIECE_VALUES[std::abs(move.capturedPiece)];
    if (stand_pat + capturedValue + 200 < alpha) {
      continue;
    }

    board.makeMove(move);

    int kingRow = board.isWhiteTurn ? board.blackKingRow : board.whiteKingRow;
    int kingCol = board.isWhiteTurn ? board.blackKingCol : board.whiteKingCol;

    if (is_square_attacked(board, kingRow, kingCol, board.isWhiteTurn)) {
      board.unmakeMove(move);
      continue;
    }
    int eval = -quiescence(board, -beta, -alpha, ply + 1);
    board.unmakeMove(move);

    if (eval >= beta) {
      return beta;
    }
    if (eval > alpha) {
      alpha = eval;
    }
  }

  return alpha;
}

int negamax(Board &board, int depth, int alpha, int beta, int ply,
            std::vector<uint64_t> &history, std::vector<Move> &pvLine) {
  nodeCount.fetch_add(1, std::memory_order_relaxed);

  if (should_stop()) {
    return 0;
  }

  bool pvNode = (beta - alpha) > 1;
  bool firstMove = true;
  const int alphaOrig = alpha;
  int maxEval = -200000;
  int MATE_VALUE = 100000;

  if (depth <= 0) {
    pvLine.clear();
    return quiescence(board, alpha, beta, ply);
  }

  if (is_threefold_repetition(history)) {
    pvLine.clear();
    return 0;
  }
  bool inCheck = is_square_attacked(
      board, board.isWhiteTurn ? board.whiteKingRow : board.blackKingRow,
      board.isWhiteTurn ? board.whiteKingCol : board.blackKingCol,
      !board.isWhiteTurn);
  uint64_t currentHash = position_key(board);
  int ttScore = 0;
  int ttDepth = 0;
  TTFlag ttFlag = TTFlag::EXACT;
  Move ttMove;
  const bool ttHit =
      globalTT.probe(currentHash, ttScore, ttDepth, ttFlag, ttMove);

  int movesSearched = 0;
  int eval = -MATE_VALUE;
  bool is_repetition_candidate = false;

  if (history.size() > 1) {
    for (int i = static_cast<int>(history.size()) - 2; i >= 0; i--) {
      if (history[i] == currentHash) {
        is_repetition_candidate = true;
        break;
      }
    }
  }

  const int staticEval = evaluate_board(board);
  if (!is_repetition_candidate && ttHit && ttDepth >= depth) {
    if (ttFlag == EXACT) {
      pvLine.clear();
      return ttScore;
    }

    if (!is_repetition_candidate && ttFlag == ALPHA && ttScore <= alpha) {
      pvLine.clear();
      return alpha;
    }
    if (!is_repetition_candidate && ttFlag == BETA && ttScore >= beta) {
      pvLine.clear();
      return beta;
    }
  }

  if ((beta - alpha) == 1 && depth < 9 && !inCheck && beta < MATE_SCORE - 100) {
    int margin = 80 * depth;

    if (staticEval - margin >= beta) {
      pvLine.clear();
      return beta;
    }
  }

  {
    int kingRow = board.isWhiteTurn ? board.whiteKingRow : board.blackKingRow;
    int kingCol = board.isWhiteTurn ? board.whiteKingCol : board.blackKingCol;
    bool inCheckLocal =
        is_square_attacked(board, kingRow, kingCol, !board.isWhiteTurn);

    if (!inCheckLocal && depth >= 3 && (beta - alpha == 1)) {
      board.isWhiteTurn = !board.isWhiteTurn;

      uint64_t nullHash = position_key(board);
      history.push_back(nullHash);

      int R = std::min(3, std::max(1, depth - 2));
      std::vector<Move> nullPv;
      int nullScore = -negamax(board, depth - 1 - R, -beta, -beta + 1, ply + 1,
                               history, nullPv);

      if (!history.empty())
        history.pop_back();
      board.isWhiteTurn = !board.isWhiteTurn;

      if (nullScore >= beta) {
        pvLine.clear();
        return beta;
      }
    }
  }

  std::vector<Move> possibleMoves = get_all_moves(board, board.isWhiteTurn);
  Move bestMove = possibleMoves.empty() ? Move() : possibleMoves[0];

  if (possibleMoves.empty()) {
    if (is_square_attacked(
            board, board.isWhiteTurn ? board.whiteKingRow : board.blackKingRow,
            board.isWhiteTurn ? board.whiteKingCol : board.blackKingCol,
            !board.isWhiteTurn))
      return -MATE_VALUE + ply;
    return 0;
  }

  std::sort(possibleMoves.begin(), possibleMoves.end(),
            [&](const Move &a, const Move &b) {
              const Move *ttMovePtr = ttHit ? &ttMove : nullptr;
              return scoreMove(board, a, ply, ttMovePtr) >
                     scoreMove(board, b, ply, ttMovePtr);
            });

  for (Move &move : possibleMoves) {
    int lmpCount = (3 * depth * depth) + 4;
    if (!pvNode && depth > 3 && depth < 8 && movesSearched >= lmpCount &&
        !inCheck && move.promotion == 0 && move.capturedPiece == 0) {
      if (!move.isEnPassant) {
        bool isKiller = false;
        if (ply < 100) {
          if (move.fromCol == killerMove[0][ply].fromCol &&
              move.fromRow == killerMove[0][ply].fromRow &&
              move.toCol == killerMove[0][ply].toCol &&
              move.toRow == killerMove[0][ply].toRow)
            isKiller = true;
          else if (move.fromCol == killerMove[1][ply].fromCol &&
                   move.fromRow == killerMove[1][ply].fromRow &&
                   move.toCol == killerMove[1][ply].toCol &&
                   move.toRow == killerMove[1][ply].toRow)
            isKiller = true;
        }

        if (!isKiller) {
          continue;
        }
      }
    }

    board.makeMove(move);
    movesSearched++;
    std::vector<Move> childPv;
    uint64_t newHash = position_key(board);
    history.push_back(newHash);
    if (firstMove) {
      eval =
          -negamax(board, depth - 1, -beta, -alpha, ply + 1, history, childPv);
      firstMove = false;
    } else {
      int reduction = 0;
      std::vector<Move> nullWindowPv;
      if (move.capturedPiece == 0 && !move.isEnPassant && move.promotion == 0 &&
          depth >= 3 && movesSearched > 4) {
        reduction = 1 + (depth / 6);

        if (depth - 1 - reduction < 1)
          reduction = depth - 2;
      }

      eval = -negamax(board, depth - 1 - reduction, -alpha - 1, -alpha, ply + 1,
                      history, nullWindowPv);

      if (reduction > 0 && eval > alpha) {
        eval = -negamax(board, depth - 1, -alpha - 1, -alpha, ply + 1, history,
                        childPv);
      }

      if (eval > alpha && eval < beta) {
        childPv.clear();
        eval = -negamax(board, depth - 1, -beta, -alpha, ply + 1, history,
                        childPv);
      } else {
        childPv.clear();
      }
    }
    if (!history.empty())
      history.pop_back();
    board.unmakeMove(move);

    if (eval > maxEval) {
      maxEval = eval;
      bestMove = move;
      pvLine.clear();
      pvLine.push_back(move);
      pvLine.insert(pvLine.end(), childPv.begin(), childPv.end());
    }

    if (eval > alpha) {
      alpha = eval;
    }

    if (beta <= alpha) {
      if (move.capturedPiece == 0 && !move.isEnPassant && move.promotion == 0) {
        if (ply < 100 && !(move.fromCol == killerMove[0][ply].fromCol &&
                           move.fromRow == killerMove[0][ply].fromRow &&
                           move.toCol == killerMove[0][ply].toCol &&
                           move.toRow == killerMove[0][ply].toRow)) {
          killerMove[1][ply] = killerMove[0][ply];
          killerMove[0][ply] = move;
        }
      }
      int from = move.fromRow * 8 + move.fromCol;
      int to = move.toRow * 8 + move.toCol;

      if (from >= 0 && from < 64 && to >= 0 && to < 64) {
        historyTable[from][to] += depth * depth;
      }

      break;
    }
  }

  TTFlag flag;
  if (maxEval <= alphaOrig)
    flag = ALPHA;
  else if (maxEval >= beta)
    flag = BETA;
  else
    flag = EXACT;

  globalTT.store(currentHash, maxEval, depth, flag, bestMove);
  return maxEval;
}

Move getBestMove(Board &board, int maxDepth, int movetimeMs,
                 const std::vector<uint64_t> &history, int ply) {
  resetNodeCounter();
  stop_search.store(false, std::memory_order_relaxed);

  auto now = std::chrono::steady_clock::now();
  start_time_ms.store(std::chrono::duration_cast<std::chrono::milliseconds>(
                          now.time_since_epoch())
                          .count(),
                      std::memory_order_relaxed);
  if (movetimeMs > 0) {
    is_time_limited.store(true, std::memory_order_relaxed);
    long long time_limit = movetimeMs;
    if (time_limit > 50)
      time_limit -= 20;
    time_limit_ms.store(time_limit, std::memory_order_relaxed);
  } else {
    is_time_limited.store(false, std::memory_order_relaxed);
    time_limit_ms.store(0, std::memory_order_relaxed);
  }

  bool isWhite = board.isWhiteTurn;
  std::vector<Move> possibleMoves = get_all_moves(board, isWhite);
  std::sort(possibleMoves.begin(), possibleMoves.end(),
            [&](const Move &a, const Move &b) {
              return scoreMove(board, a, 0, nullptr) >
                     scoreMove(board, b, 0, nullptr);
            });
  if (possibleMoves.empty())
    return {};
  if (possibleMoves.size() == 1)
    return possibleMoves[0];

  Move bestMoveSoFar = possibleMoves[0];

  auto gSearchStart = std::chrono::steady_clock::now();
  auto gTimeLimited = (movetimeMs > 0);
  auto gTimeLimitMs =
      gTimeLimited ? movetimeMs : std::numeric_limits<int>::max();
  const int effectiveMaxDepth = gTimeLimited ? 128 : maxDepth;
  int bestValue;

  std::vector<uint64_t> localHistory;
  localHistory.reserve(std::min((size_t)100, history.size()) + 1);
  auto startIt =
      (history.size() > 100) ? (history.end() - 100) : history.begin();
  localHistory.assign(startIt, history.end());

  int lastScore = 0;

  for (int depth = 1; depth <= effectiveMaxDepth; depth++) {
    if (stop_search.load(std::memory_order_relaxed))
      break;

    int delta = 50;
    int alpha = -2000000000;
    int beta = 2000000000;

    if (depth >= 5) {
      alpha = std::max(-2000000000, lastScore - delta);
      beta = std::min(2000000000, lastScore + delta);
    }
    while (true) {
      const int alphaStart = alpha;
      const int betaStart = beta;
      bestValue = std::numeric_limits<int>::min() / 2;
      Move currentDepthBestMove;
      bool thisDepthCompleted = true;

      if (depth > 1) {
        Move pvMove = bestMoveSoFar;
        std::stable_sort(
            possibleMoves.begin(), possibleMoves.end(),
            [&](const Move &a, const Move &b) {
              const bool aIsPV =
                  (a.fromRow == pvMove.fromRow && a.fromCol == pvMove.fromCol &&
                   a.toRow == pvMove.toRow && a.toCol == pvMove.toCol &&
                   a.promotion == pvMove.promotion);
              const bool bIsPV =
                  (b.fromRow == pvMove.fromRow && b.fromCol == pvMove.fromCol &&
                   b.toRow == pvMove.toRow && b.toCol == pvMove.toCol &&
                   b.promotion == pvMove.promotion);

              if (aIsPV != bIsPV)
                return aIsPV;

              const int sa = scoreMove(board, a, 0, nullptr);
              const int sb = scoreMove(board, b, 0, nullptr);
              if (sa != sb)
                return sa > sb;

              if (a.fromRow != b.fromRow)
                return a.fromRow < b.fromRow;
              if (a.fromCol != b.fromCol)
                return a.fromCol < b.fromCol;
              if (a.toRow != b.toRow)
                return a.toRow < b.toRow;
              if (a.toCol != b.toCol)
                return a.toCol < b.toCol;
              return a.promotion < b.promotion;
            });
      }

      for (Move move : possibleMoves) {
        if (stop_search.load(std::memory_order_relaxed)) {
          thisDepthCompleted = false;
          break;
        }

        board.makeMove(move);

        std::vector<Move> childPv;

        uint64_t newHash = position_key(board);
        localHistory.push_back(newHash);
        int val = -negamax(board, depth - 1, -beta, -alpha, ply + 1,
                           localHistory, childPv);
        localHistory.pop_back();

        board.unmakeMove(move);

        if (stop_search.load(std::memory_order_relaxed)) {
          thisDepthCompleted = false;
          break;
        }

        if (val > bestValue) {
          bestValue = val;
          currentDepthBestMove = move;

          if (bestValue > alpha) {
            alpha = bestValue;

            if (alpha >= beta) {
              break;
            }
          }

          if (stop_search.load(std::memory_order_relaxed)) {
            break;
          }

          if (thisDepthCompleted) {
            lastScore = bestValue;
            bestMoveSoFar = currentDepthBestMove;
            auto searchEnd = std::chrono::steady_clock::now();
            long long duration =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    searchEnd - gSearchStart)
                    .count();
            std::cout << "info depth " << depth << " score cp " << bestValue
                      << " time " << duration << " pv "
                      << move_to_uci(bestMoveSoFar) << " ";
            for (const Move &pvMove : childPv) {
              std::cout << move_to_uci(pvMove) << " ";
            }
            std::cout << std::endl;
          }
        }
      }

      if (stop_search.load(std::memory_order_relaxed)) {
        break;
      }

      if (depth >= 5 && (bestValue <= alphaStart || bestValue >= betaStart)) {
        alpha = std::max(-2000000000, bestValue - delta);
        beta = std::min(2000000000, bestValue + delta);
        delta += delta / 2;
        continue;
      }

      if (thisDepthCompleted &&
          (bestValue > std::numeric_limits<int>::min() / 2)) {
        bestMoveSoFar = currentDepthBestMove;
      }
      break;
    }

    if (stop_search.load(std::memory_order_relaxed) ||
        bestValue >= 100000 - 50) {
      break;
    }
  }

  return bestMoveSoFar;
}
