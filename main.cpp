#include "board.h"
#include "bitboard.h"
#include "search.h"
#include "evaluation.h"
#include <iostream>
#include <string>
#include <sstream>
#include <chrono>
#include <vector>
#include <algorithm>
#include <thread>
#include <atomic>

#define VERSION "1.2.1"

static uint64_t perft(Board& board, int depth) {
    if (depth <= 0) return 1ULL;

    uint64_t nodes = 0;
    std::vector<Move> moves = get_all_moves(board, board.isWhiteTurn);
    for (Move& move : moves) {
        board.makeMove(move);

        int kingRow = 0;
        int kingCol = 0;
        if (!king_square(board, !board.isWhiteTurn, kingRow, kingCol)) {
            board.unmakeMove(move);
            continue;
        }
        const bool illegal = is_square_attacked(board, kingRow, kingCol, board.isWhiteTurn);

        if (!illegal) {
            nodes += perft(board, depth - 1);
        }

        board.unmakeMove(move);
    }

    return nodes;
}

void bench() {
    const int benchDepth = 7;
    const std::vector<std::string> fens = {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        "r1bq1rk1/ppp2ppp/2n1pn2/2b5/4P3/2NP1N2/PPP1BPPP/R1BQ1RK1 w - - 0 1",
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R b KQkq - 0 1",
        "r2q1rk1/1bp1bppp/p1np1n2/1p6/3NP3/1BN1BP2/PPP1B1PP/R2Q1RK1 w - - 0 10",
        "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
        "4rrk1/pppb2bp/3p2p1/3Pnp2/2P1q3/2N1P1P1/PP1Q1PBP/R3R1K1 w - - 0 1"
    };

    auto move_to_uci = [](const Move& m) {
        if (m.fromRow == 0 && m.fromCol == 0 && m.toRow == 0 && m.toCol == 0 && m.promotion == 0) return std::string("0000");
        std::string s;
        s += columns[m.fromCol];
        s += static_cast<char>('0' + (8 - m.fromRow));
        s += columns[m.toCol];
        s += static_cast<char>('0' + (8 - m.toRow));
        if (m.promotion != 0) {
            switch (std::abs(m.promotion)) {
                case queen: s += 'q'; break;
                case rook: s += 'r'; break;
                case bishop: s += 'b'; break;
                case knight: s += 'n'; break;
                default: break;
            }
        }
        return s;
    };

    uint64_t totalNodes = 0;
    long long totalTimeMs = 0;

    Board board;
    if (globalTT.entryCount() == 0) globalTT.resize(16);
    globalTT.clear();

    for (size_t i = 0; i < fens.size(); ++i) {
        board.loadFromFEN(fens[i]);
        std::vector<uint64_t> history;
        history.reserve(64);
        history.push_back(position_key(board));

        resetNodeCounter();
        auto startTime = std::chrono::steady_clock::now();
        Move best = getBestMove(board, benchDepth, -1, history);
        auto endTime = std::chrono::steady_clock::now();

        long long elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
        long long nodes = getNodeCounter();

        totalNodes += static_cast<uint64_t>(nodes);
        totalTimeMs += elapsed;

        long long nps = (elapsed > 0) ? (nodes * 1000 / elapsed) : (nodes * 1000);
        std::cout << "info string bench pos " << (i + 1)
              << " depth " << benchDepth
              << " time " << elapsed << "ms"
              << " nodes " << nodes
              << " nps " << nps
              << " best " << move_to_uci(best)
              << std::endl;
    }

    long long safeMs = std::max<long long>(1, totalTimeMs);
    long long totalNps = (totalNodes * 1000) / safeMs;
    std::cout << "bench total nodes " << totalNodes
              << " time " << safeMs << "ms nps " << totalNps
              << std::endl;
    std::cout << "Bench: " << totalNodes << std::endl;
}

int main(int argc, char* argv[]) {
    std::cout.setf(std::ios::unitbuf); // Disable output buffering
    init_all();
    initLMRtables();
    if (argc > 1 && std::string(argv[1]) == "bench") {
        bench();
        return 0;
    }
    else if (argc > 1 && (std::string(argv[1]) == "version" || std::string(argv[1]) == "--version" || std::string(argv[1]) == "-v")) {
        std::cout << "SoloEngine version " << VERSION << std::endl;
        return 0;
    }


    Board board;
    // Default TT size matches the UCI 'Hash' option default.
    if (globalTT.entryCount() == 0) globalTT.resize(128);
    std::vector<uint64_t> gameHistory;
    gameHistory.reserve(512);
    std::string line;

    std::thread searchThread;
    std::atomic<bool> searchRunning{false};

    auto join_search_if_done = [&]() {
        if (!searchRunning.load(std::memory_order_relaxed) && searchThread.joinable()) {
            searchThread.join();
        }
    };

    auto stop_and_join_search = [&]() {
        if (searchRunning.load(std::memory_order_relaxed)) {
            request_stop_search();
        }
        if (searchThread.joinable()) {
            searchThread.join();
        }
        searchRunning.store(false, std::memory_order_relaxed);
    };

    // Normal UCI loop continues from here...
    while (std::getline(std::cin, line)) {

        // If a previous search finished, clean up the thread.
        join_search_if_done();

        if (line == "stop") {
            // GUI/OpenBench may send this when time is up.
            request_stop_search();
            continue;
        }
        
        if (line == "uci") {
            std::cout << "id name SoloEngine" << std::endl;
            std::cout << "id author xsolod3v" << std::endl;
            std::cout << "option name Hash type spin default 128 min 1 max 2048" << std::endl;
            std::cout << "option name Threads type spin default 1 min 1 max 8" << std::endl;
            std::cout << "option name UseTT type check default true" << std::endl;
            std::cout << "uciok" << std::endl;
        }
        
        else if (line == "isready") {
            std::cout << "readyok" << std::endl;
        }

        else if (line == "bench") {
            bench();
        }
        else if (line.rfind("setoption", 0) == 0) {
            std::stringstream ss(line);
            std::string token;
            std::string name;
            std::string value;
            ss >> token; // setoption
            while (ss >> token) {
                if (token == "name") {
                    name.clear();
                    while (ss >> token && token != "value") {
                        if (!name.empty()) name += " ";
                        name += token;
                    }
                    if (token != "value") break;
                }
                if (token == "value") {
                    std::getline(ss, value);
                    if (!value.empty() && value[0] == ' ') value.erase(0, 1);
                    break;
                }
            }
            if (name == "Hash") {
                int mb = std::max(1, std::stoi(value));
                globalTT.resize(mb);
                globalTT.clear();
            } else if (name == "UseTT") {
                std::string v = value;
                std::transform(v.begin(), v.end(), v.begin(), ::tolower);
                set_use_tt(v == "true" || v == "1" || v == "on");
            }
        }
        
        else if (line.rfind("perft", 0) == 0) {
            stop_and_join_search();
            std::stringstream ss(line);
            std::string token;
            int depth = 0;
            ss >> token >> depth;
            if (depth <= 0) {
                std::cout << "info string perft depth missing or invalid" << std::endl;
            } else {
                uint64_t nodes = perft(board, depth);
                std::cout << "perft " << depth << " nodes " << nodes << std::endl;
            }
        }

        else if (line == "ucinewgame") {
            stop_and_join_search();
            globalTT.clear();
            board.resetBoard();
            gameHistory.clear();
            gameHistory.push_back(position_key(board));
            clear_search_heuristics();
        }

        else if (line.substr(0, 8) == "position") {
            stop_and_join_search();
            if (line.find("startpos") != std::string::npos) {
                board.resetBoard();
            }
            else if (line.find("fen") != std::string::npos) {
                size_t fenStart = line.find("fen") + 4;
                size_t movesPos = line.find("moves");
                std::string fenStr;
                if (movesPos != std::string::npos) {
                    fenStr = line.substr(fenStart, movesPos - fenStart);
                } else {
                    fenStr = line.substr(fenStart);
                }
                board.loadFromFEN(fenStr);
            }
            gameHistory.clear();
            gameHistory.push_back(position_key(board));
            
            size_t movesPos = line.find("moves");
            if (movesPos != std::string::npos) {

                std::string movesStr = line.substr(movesPos + 6);
                std::stringstream ss(movesStr);
                std::string moveToken;
                
                while (ss >> moveToken) {
                    Move m = uci_to_move(moveToken);
                    board.makeMove(m);
                    gameHistory.push_back(position_key(board));
                }
            }
        }

        else if (line.substr(0, 2) == "go") {
            stop_and_join_search();

            int depth = -1;
            int movetime = -1;
            int wtime = -1, btime = -1;
            int winc = 0, binc = 0;
            {
                std::stringstream ss(line);
                std::string token;
                ss >> token;
                while (ss >> token) {
                    if (token == "depth") {
                        ss >> depth;
                    } else if (token == "movetime") {
                        ss >> movetime;
                    } else if (token == "wtime") {
                        ss >> wtime;
                    } else if (token == "btime") {
                        ss >> btime;
                    } else if (token == "winc") {
                        ss >> winc;
                    } else if (token == "binc") {
                        ss >> binc;
                    }
                }
            }

            int timeToThink = -1;
            int searchDepth = depth;

            if (movetime != -1) {
                timeToThink = movetime;
                searchDepth = 128;
            } else if (wtime != -1 || btime != -1) {
                const int myTime = board.isWhiteTurn ? wtime : btime;
                const int myInc = board.isWhiteTurn ? winc : binc;

                // Simple TC: allocate a fraction of remaining time + some increment.
                // IMPORTANT: enforce a small minimum (currently 10 ms) so the engine always has
                // some time to search and we avoid pathological cases where a 0-ms allocation
                // would effectively disable time management in the search.
                if (myTime > 0) {
                    timeToThink = (myTime / 20) + (myInc / 2);
                    if (timeToThink >= myTime) {
                        timeToThink = myTime - 50;
                    }
                } else {
                    timeToThink = 10;
                }
                if (timeToThink < 10) timeToThink = 10;
                searchDepth = 128;
            } else if (searchDepth == -1) {
                searchDepth = 6;
            }

            // Run search on a worker thread so we can still react to `stop` / `isready`.
            searchRunning.store(true, std::memory_order_relaxed);
            searchThread = std::thread([&board, &gameHistory, searchDepth, timeToThink, &searchRunning]() {
                Move best = getBestMove(board, searchDepth, timeToThink, gameHistory);

                // If no legal move was found (mate/stalemate), output UCI null move.
                if (best.fromRow == 0 && best.fromCol == 0 && best.toRow == 0 && best.toCol == 0 && best.promotion == 0) {
                    std::cout << "bestmove 0000" << std::endl;
                } else {
                    std::cout << "bestmove "
                              << columns[best.fromCol] << (8 - best.fromRow)
                              << columns[best.toCol] << (8 - best.toRow);
                    if (best.promotion != 0) {
                        switch (abs(best.promotion)) {
                            case queen: std::cout << 'q'; break;
                            case rook: std::cout << 'r'; break;
                            case bishop: std::cout << 'b'; break;
                            case knight: std::cout << 'n'; break;
                            default: break;
                        }
                    }
                    std::cout << std::endl;
                }
                searchRunning.store(false, std::memory_order_relaxed);
            });
        }
        else if (line == "quit") {
            request_stop_search();
            stop_and_join_search();
            break;
        }
    }

    // Clean shutdown if stdin closes.
    request_stop_search();
    stop_and_join_search();
    return 0;
}
