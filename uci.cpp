#include "uci.h"

#include "board.h"
#include "search.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

extern void bench();

namespace {

struct SearchParams {
    int depth = -1;
    int movetime = -1;
    int wtime = -1;
    int btime = -1;
    int winc = 0;
    int binc = 0;
};

struct SearchLimits {
    int searchDepth = -1;
    int timeToThink = -1;
};

uint64_t perft(Board& board, int depth) {
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

class UciSession {
public:
    explicit UciSession(std::string engineVersion) : version(std::move(engineVersion)) {
        if (globalTT.entryCount() == 0) globalTT.resize(128);
        gameHistory.reserve(512);
    }

    ~UciSession() {
        request_stop_search();
        stop_and_join_search();
    }

    int run() {
        std::string line;
        while (std::getline(std::cin, line)) {
            join_search_if_done();
            if (!dispatch(line)) {
                return 0;
            }
        }
        return 0;
    }

private:
    bool dispatch(const std::string& line) {
        if (line == "stop") {
            request_stop_search();
            return true;
        }

        if (line == "uci") {
            std::cout << "id name SoloEngine " << version << std::endl;
            std::cout << "id author Yunus Emre Yıldız" << std::endl;
            std::cout << "option name Hash type spin default 128 min 1 max 2048" << std::endl;
            std::cout << "option name Threads type spin default 1 min 1 max 8" << std::endl;
            std::cout << "option name UseTT type check default true" << std::endl;
            std::cout << "uciok" << std::endl;
            return true;
        }

        if (line == "isready") {
            std::cout << "readyok" << std::endl;
            return true;
        }

        if (line == "bench") {
            bench();
            return true;
        }

        if (line.rfind("setoption", 0) == 0) {
            handle_setoption(line);
            return true;
        }

        if (line.rfind("perft", 0) == 0) {
            handle_perft(line);
            return true;
        }

        if (line == "ucinewgame") {
            handle_ucinewgame();
            return true;
        }

        if (line.rfind("position", 0) == 0) {
            handle_position(line);
            return true;
        }

        if (line.rfind("go", 0) == 0) {
            handle_go(line);
            return true;
        }

        if (line == "quit") {
            request_stop_search();
            stop_and_join_search();
            return false;
        }

        return true;
    }

    void join_search_if_done() {
        if (!searchRunning.load(std::memory_order_relaxed) && searchThread.joinable()) {
            searchThread.join();
        }
    }

    void stop_and_join_search() {
        if (searchRunning.load(std::memory_order_relaxed)) {
            request_stop_search();
        }
        if (searchThread.joinable()) {
            searchThread.join();
        }
        searchRunning.store(false, std::memory_order_relaxed);
    }

    void handle_setoption(const std::string& line) {
        std::stringstream ss(line);
        std::string token;
        std::string name;
        std::string value;
        ss >> token;
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

    void handle_perft(const std::string& line) {
        stop_and_join_search();

        std::stringstream ss(line);
        std::string token;
        int depth = 0;
        ss >> token >> depth;

        if (depth <= 0) {
            std::cout << "info string perft depth missing or invalid" << std::endl;
            return;
        }

        uint64_t nodes = perft(board, depth);
        std::cout << "perft " << depth << " nodes " << nodes << std::endl;
    }

    void handle_ucinewgame() {
        stop_and_join_search();
        globalTT.clear();
        board.resetBoard();
        gameHistory.clear();
        gameHistory.push_back(position_key(board));
        clear_search_heuristics();
    }

    void handle_position(const std::string& line) {
        stop_and_join_search();

        if (line.find("startpos") != std::string::npos) {
            board.resetBoard();
        } else if (line.find("fen") != std::string::npos) {
            const size_t fenStart = line.find("fen") + 4;
            const size_t movesPos = line.find("moves");
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

        const size_t movesPos = line.find("moves");
        if (movesPos == std::string::npos) return;

        std::string movesStr = line.substr(movesPos + 6);
        std::stringstream ss(movesStr);
        std::string moveToken;
        while (ss >> moveToken) {
            Move m = uci_to_move(moveToken);
            board.makeMove(m);
            gameHistory.push_back(position_key(board));
        }
    }

    SearchParams parse_search_params(const std::string& line) const {
        SearchParams params;
        std::stringstream ss(line);
        std::string token;
        ss >> token;

        while (ss >> token) {
            if (token == "depth") {
                ss >> params.depth;
            } else if (token == "movetime") {
                ss >> params.movetime;
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
        return params;
    }

    SearchLimits compute_time_to_think(const SearchParams& params) const {
        SearchLimits limits;
        limits.searchDepth = params.depth;
        limits.timeToThink = -1;

        if (params.movetime != -1) {
            limits.timeToThink = params.movetime;
            limits.searchDepth = 128;
            return limits;
        }

        if (params.wtime != -1 || params.btime != -1) {
            const int myTime = board.isWhiteTurn ? params.wtime : params.btime;
            const int myInc = board.isWhiteTurn ? params.winc : params.binc;

            if (myTime > 0) {
                limits.timeToThink = (myTime / 20) + (myInc / 2);
                if (limits.timeToThink >= myTime) {
                    limits.timeToThink = myTime - 50;
                }
            } else {
                limits.timeToThink = 10;
            }

            if (limits.timeToThink < 10) limits.timeToThink = 10;
            limits.searchDepth = 128;
            return limits;
        }

        if (limits.searchDepth == -1) {
            limits.searchDepth = 6;
        }

        return limits;
    }

    static std::string move_to_uci(const Move& move) {
        if (move.fromRow == 0 && move.fromCol == 0 && move.toRow == 0 &&
            move.toCol == 0 && move.promotion == 0) {
            return "0000";
        }

        std::string out;
        out += columns[move.fromCol];
        out += static_cast<char>('0' + (8 - move.fromRow));
        out += columns[move.toCol];
        out += static_cast<char>('0' + (8 - move.toRow));

        if (move.promotion != 0) {
            switch (std::abs(move.promotion)) {
                case QUEEN: out += 'q'; break;
                case ROOK: out += 'r'; break;
                case BISHOP: out += 'b'; break;
                case KNIGHT: out += 'n'; break;
                default: break;
            }
        }

        return out;
    }

    void handle_go(const std::string& line) {
        stop_and_join_search();

        const SearchParams params = parse_search_params(line);
        const SearchLimits limits = compute_time_to_think(params);

        searchRunning.store(true, std::memory_order_relaxed);
        searchThread = std::thread([this, limits]() {
            Move best = getBestMove(board, limits.searchDepth, limits.timeToThink, gameHistory);
            std::cout << "bestmove " << move_to_uci(best) << std::endl;
            searchRunning.store(false, std::memory_order_relaxed);
        });
    }

    const std::string version;
    Board board;
    std::vector<uint64_t> gameHistory;
    std::thread searchThread;
    std::atomic<bool> searchRunning{false};
};

} // namespace

int run_uci_loop(const std::string& version) {
    UciSession session(version);
    return session.run();
}
