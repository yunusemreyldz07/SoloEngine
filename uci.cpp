#include "uci.h"
#include "board.h"
#include "bitboard.h"
#include "search.h"
#include "evaluation.h"
#include "history.h"
#include <iostream>
#include <string>
#include <sstream>
#include <chrono>
#include <thread>
#include <algorithm>

#define VERSION "1.4.0"

TranspositionTable ttTable;

struct UciTimeParams {
    int wtime = -1;
    int btime = -1;
    int winc = 0;
    int binc = 0;
};

struct SearchLimit {
    int timeToThink = -1;
    int depthLimit = -1;
};

std::string move_to_uci(const Move m) {
        if (m == 0) return std::string("0000");
        std::string s;
        s += columns[sq_to_col(move_from(m))];
        s += static_cast<char>('0' + (8 - sq_to_row(move_from(m))));
        s += columns[sq_to_col(move_to(m))];
        s += static_cast<char>('0' + (8 - sq_to_row(move_to(m))));
        if (get_promotion_type(m) != -1) {
            switch (get_promotion_type(m)) {
                case QUEEN: s += 'q'; break;
                case ROOK: s += 'r'; break;
                case BISHOP: s += 'b'; break;
                case KNIGHT: s += 'n'; break;
                default: break;
            }
        }
        return s;
    };

void bench() {
    const int benchDepth = 8;
    
    // Diverse set of positions covering opening, middlegame, endgame, and tactical themes
    const std::vector<std::string> fens = {
        // Opening
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        "r1bqkbnr/pppp1ppp/2n5/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R w KQkq - 2 3",
        
        // Complex middlegame
        "r1bq1rk1/ppp2ppp/2n1pn2/2b5/4P3/2NP1N2/PPP1BPPP/R1BQ1RK1 w - - 0 1",
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
        "r2q1rk1/1bp1bppp/p1np1n2/1p6/3NP3/1BN1BP2/PPP1B1PP/R2Q1RK1 w - - 0 10",
        
        // Tactical
        "r1b1k2r/ppppnppp/2n2q2/2b5/3NP3/2P1B3/PP3PPP/RN1QKB1R w KQkq - 0 1",
        "2rr2k1/1p3ppp/pq2pn2/4N3/3P4/1B6/PP2QPPP/3R1RK1 w - - 0 1",
        
        // Endgame
        "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
        "8/8/4kpp1/3p1b2/p6P/2B5/6P1/6K1 w - - 0 1",
        "8/5pk1/6p1/3P4/5PP1/8/8/6K1 w - - 0 1",
        
        // Imbalanced
        "4rrk1/pppb2bp/3p2p1/3Pnp2/2P1q3/2N1P1P1/PP1Q1PBP/R3R1K1 w - - 0 1",
        "r1bq1rk1/pp2ppbp/2np1np1/8/3NP3/2N1B3/PPP1BPPP/R2Q1RK1 w - - 0 9",
    };

    uint64_t totalNodes = 0;
    long long totalTimeMs = 0;

    Board board;
    if (ttTable.count() == 0) ttTable.resize(128);
    ttTable.clear();

    for (size_t i = 0; i < fens.size(); ++i) {
        clear_history();
        board.loadFEN(fens[i]);
        std::vector<uint64_t> positionHistory;
        positionHistory.reserve(64);
        positionHistory.push_back(position_key(board));

        resetNodeCounter();
        auto startTime = std::chrono::steady_clock::now();
        Move best = getBestMove(board, benchDepth);
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

static uint64_t perft(Board& board, int depth) {
    if (depth <= 0) return 1ULL;

    uint64_t nodes = 0;
    int moveCount = 0;
    Move moves[256];
    get_all_moves(board, moves, moveCount);
    for (int i = 0; i < moveCount; i++) {
        board.makeMove(moves[i]);
        nodes += perft(board, depth - 1);
        board.unmakeMove(moves[i]);
    }

    return nodes;
}

int handle_uci_commands(int argc, char* argv[]){
    std::cout.setf(std::ios::unitbuf);

    if (argc > 1 && std::string(argv[1]) == "bench") {
        bench();
        return 0;
    }
    else if (argc > 1 && std::string(argv[1]) == "--version") {
        std::cout << "SoloEngine version " << VERSION << std::endl;
        return 0;
    }

    
    Board board;
    // Default TT size matches the UCI 'Hash' option default.
    if (ttTable.count() == 0) ttTable.resize(128);
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
        if (searchThread.joinable()) {
            searchThread.join();
        }
        searchRunning.store(false, std::memory_order_relaxed);
    };

    
    // Normal UCI loop continues from here...
    while (std::getline(std::cin, line)) {

        UciTimeParams params;
        SearchLimit limits;
        // If a previous search finished, clean up the thread.
        join_search_if_done();

        if (line == "stop") {
            // Stop command is ignored in this simplified search mode.
            continue;
        }
        
        if (line == "uci") {
            std::cout << "id name SoloEngine " << VERSION << std::endl;
            std::cout << "id author Yunus Emre Yıldız" << std::endl;
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
                ttTable.resize(mb);
                ttTable.clear();


            } else if (name == "UseTT") {
                // Ignored in simplified search mode.
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
            ttTable.clear();
            board.reset();
            clear_history();
            gameHistory.clear();
            gameHistory.push_back(position_key(board));
        }

        else if (line.substr(0, 8) == "position") {
            stop_and_join_search();
            if (line.find("startpos") != std::string::npos) {
                board.reset();
                clear_history();
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
                board.loadFEN(fenStr);
            }
            gameHistory.clear();
            gameHistory.push_back(position_key(board));
            clear_history();
            
            size_t movesPos = line.find("moves");
            if (movesPos != std::string::npos) {

                std::string movesStr = line.substr(movesPos + 6);
                std::stringstream ss(movesStr);
                std::string moveToken;
                
                while (ss >> moveToken) {
                    Move m = uci_to_move(moveToken, board);
                    board.makeMove(m);
                    gameHistory.push_back(position_key(board));
                }
            }
        }

        else if (line.substr(0, 2) == "go") {
            stop_and_join_search();
            {
                std::stringstream ss(line);
                std::string token;
                ss >> token;
                while (ss >> token) {
                    if (token == "depth") {
                        ss >> limits.depthLimit;
                    } else if (token == "movetime") {
                        ss >> limits.timeToThink;
                    } else if (token == "wtime") {
                        ss >> params.wtime;
                    } else if (token == "btime") {
                        ss >> params.btime;
                    } else if (token == "winc") {
                        ss >> params.winc;
                    } else if (token == "binc") {
                        ss >> params.binc;
                    }
                }
            }

            // if time is not specified then use time controls if available, otherwise default to depth 8
            if (limits.timeToThink != -1) {
                limits.depthLimit = 128;
            } else if (params.wtime != -1 || params.btime != -1) { // if time controls are given, calculate time to think
                const int myTime = board.stm == WHITE ? params.wtime : params.btime;
                const int myInc = board.stm == WHITE ? params.winc : params.binc;
            
                // Time calculation

                if (myTime > 0) {
                    limits.timeToThink = (myTime / 20) + (myInc / 2);
                    if (limits.timeToThink >= myTime) {
                        limits.timeToThink = myTime - 50;
                    }
                } else {
                    limits.timeToThink = 10;
                }
                if (limits.timeToThink < 10) limits.timeToThink = 10;
                limits.depthLimit = 128;
            } else if (limits.depthLimit == -1) {
                limits.depthLimit = 8;
            }

            // Run search on a worker thread so we can still react to `stop` / `isready`.
            searchRunning.store(true, std::memory_order_relaxed);

            
            // Get the best move within the specified limits and current position history for repetition detection.
            searchThread = std::thread([&board, &gameHistory, depthLimit = limits.depthLimit, timeToThink = limits.timeToThink, &searchRunning]() {
                Move best = getBestMove(board, depthLimit, timeToThink, gameHistory);

                // If no legal move was found (mate/stalemate), output UCI null move.
                if (best == 0) {
                    std::cout << "bestmove 0000" << std::endl;
                } else {
                    std::cout << "bestmove "
                              << columns[sq_to_col(move_from(best))] << (8 - sq_to_row(move_from(best)))
                              << columns[sq_to_col(move_to(best))] << (8 - sq_to_row(move_to(best)));
                    if (get_promotion_type(best) != -1) {
                        switch (abs(get_promotion_type(best))) {
                            case QUEEN: std::cout << 'q'; break;
                            case ROOK: std::cout << 'r'; break;
                            case BISHOP: std::cout << 'b'; break;
                            case KNIGHT: std::cout << 'n'; break;
                            default: break;
                        }
                    }
                    std::cout << std::endl;
                }
                searchRunning.store(false, std::memory_order_relaxed);
            });
        }
        else if (line == "quit") {
            stop_and_join_search();
            break;
        }
    }

    // Clean shutdown if stdin closes.
    stop_and_join_search();
    return 0;

}
