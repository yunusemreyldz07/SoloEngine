#ifndef SEARCH_H
#define SEARCH_H

#include "board.h"

extern void initLMRtables();
void resetNodeCounter();
long long getNodeCounter();
void requestSearchStop();

int16_t negamax(Board& board, int depth, int16_t alpha, int16_t beta, int ply, std::vector<Move>& pvLine, std::vector<uint64_t>& positionHistory);

Move getBestMove(Board& board, int maxDepth, int movetimeMs = -1, const std::vector<uint64_t>& positionHistory = {}, int ply = 0);

enum TTFlag : uint8_t {
    TT_EXACT, // Exact Score (PV Node)
    TT_ALPHA, // Lower bound (Fail-High)
    TT_BETA   // Upper bound (Fail-Low)
};

struct SearchStack {
    Move singularMove;
    int cutOffCount;
    int16_t staticEval;
};

struct TTEntry { // 16 bytes total
    uint64_t hashKey;
    int16_t score;
    int8_t depth;
    TTFlag flag;
    Move bestMove;
};

class TranspositionTable {
private:
    std::vector<TTEntry> table;

public:
    TranspositionTable() {
        resize(128); // 128 MB default size
    }

    void resize(int mbSize) {
        // 1 MB = 1024 * 1024 byte
        size_t numEntries = (mbSize * 1024ULL * 1024ULL) / sizeof(TTEntry);
        
        table.clear();
        table.resize(numEntries);
    }

    void clear() {
        // reset all entries to default values
        for (auto& entry : table) {
            entry = TTEntry{};
        }
    }

    int count() const {
        return static_cast<int>(table.size());
    }

    // TT get entry by hash key
    TTEntry& getEntry(uint64_t hashKey) {
        return table[hashKey % table.size()];
    }

    void writeEntry(uint64_t hashKey, int16_t score, int8_t depth, TTFlag flag, Move bestMove) {
        TTEntry& entry = getEntry(hashKey);
        
        bool isNewPosition = (entry.hashKey != hashKey);

        if (isNewPosition || depth > entry.depth || (depth == entry.depth && flag == TT_EXACT)) {
            entry.hashKey = hashKey;
            entry.score = score;
            entry.depth = depth;
            entry.flag = flag;
            
            if (bestMove != 0 || isNewPosition) {
                entry.bestMove = bestMove;
            }
        }
    }
};

// Global TT
extern TranspositionTable ttTable;

#endif
