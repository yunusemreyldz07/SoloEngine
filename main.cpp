#include "board.h"
#include "bitboard.h"
#include "search.h"
#include "evaluation.h"
#include "uci.h"
#include <iostream>
#include <string>
#include <chrono>
#include <vector>
#include <algorithm>

#define VERSION "1.4.0"

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

    auto move_to_uci = [](const Move& m) {
        if (m.fromRow == 0 && m.fromCol == 0 && m.toRow == 0 && m.toCol == 0 && m.promotion == 0) return std::string("0000");
        std::string s;
        s += columns[m.fromCol];
        s += static_cast<char>('0' + (8 - m.fromRow));
        s += columns[m.toCol];
        s += static_cast<char>('0' + (8 - m.toRow));
        if (m.promotion != 0) {
            switch (m.promotion) {
                case QUEEN: s += 'q'; break;
                case ROOK: s += 'r'; break;
                case BISHOP: s += 'b'; break;
                case KNIGHT: s += 'n'; break;
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
        std::vector<uint64_t> positionHistory;
        positionHistory.reserve(64);
        positionHistory.push_back(position_key(board));

        resetNodeCounter();
        auto startTime = std::chrono::steady_clock::now();
        Move best = getBestMove(board, benchDepth, -1, positionHistory);
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
    } else if (argc > 1 && std::string(argv[1]) == "--version") {
        std::cout << "SoloEngine version " << VERSION << std::endl;
        return 0;
    }

    return run_uci_loop(VERSION);
}
