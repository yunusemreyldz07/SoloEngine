#include "board.h"
#include "search.h"
#include <iostream>
#include <string>
#include <sstream>

char columns[] = "abcdefgh";

int main() {
    Board board;
    std::vector<uint64_t> gameHistory;
    gameHistory.reserve(512);
    std::string line;

    while (std::getline(std::cin, line)) {
        
        if (line == "uci") {
            std::cout << "id name SoloBot" << std::endl;
            std::cout << "id author xsolod3v" << std::endl;
            std::cout << "uciok" << std::endl;
        }
        
        // 2. "isready" gelirse
        else if (line == "isready") {
            std::cout << "readyok" << std::endl;
        }
        
        // 3. POZİSYON GÜNCELLEME (Örn: "position startpos moves e2e4 e7e5")
        else if (line.substr(0, 8) == "position") {
            board.resetBoard(); // Tahtayı sıfırla
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
            int depth = 6;
            {
                std::stringstream ss(line);
                std::string token;
                ss >> token; // "go"
                while (ss >> token) {
                    if (token == "depth") {
                        int parsed = 0;
                        if (ss >> parsed) depth = parsed;
                    }
                }
            }
            // Before every move
            if (transpositionTable.size() > 10000000) {  // 10 million entries
                transpositionTable.clear();  // Clear
            }
            Move best = getBestMove(board, depth, gameHistory);
            
            std::cout << "bestmove " << columns[best.fromCol] << (8 - best.fromRow) 
                 << columns[best.toCol] << (8 - best.toRow);
            
            if (best.promotion != 0) {
                switch (abs(best.promotion)) {
                    case queen:
                        std::cout << 'q';
                        break;
                    case rook:
                        std::cout << 'r';
                        break;
                    case bishop:
                        std::cout << 'b';
                        break;
                    case knight:
                        std::cout << 'n';
                        break;
                }
            }
            std::cout << std::endl;
        }
        
        else if (line == "quit") {
            break;
        }
    }
    return 0;
}