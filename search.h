#ifndef SEARCH_H
#define SEARCH_H

#include "board.h"
#include <vector>
#include <cstdint>
#include <atomic>
#include <cstring>

extern char columns[];
extern Move killerMove[2][MAX_PLY]; // 2 slots
extern int historyTable[64][64];    // fromSquare x toSquare
extern std::atomic<long long> nodeCount; // visited node counter
extern int LMR_TABLE[256][256];     // Late Move Reduction table

extern void initLMRtables();

// Tunable search knobs for quick A/B testing.
struct SearchParams {
	bool use_lmr = true;          // Late Move Reductions
	bool use_lmp = true;          // Late Move Pruning
	bool use_aspiration = true;   // Aspiration windows in iterative deepening
	bool use_qsearch_see = true; // SEE-based pruning inside quiescence

	int lmp_min_depth = 4;        // Minimum depth to consider LMP
	int lmp_max_depth = 8;        // Maximum depth to consider LMP

	int aspiration_delta = 50;    // Initial aspiration half-window in centipawns
};

const SearchParams& get_search_params();
void set_search_params(const SearchParams& params);

void resetNodeCounter();
long long getNodeCounter();

// Move ordering
int scoreMove(const Board& board, const Move& move, int ply, const Move* ttMove);

// Search functions (PV enabled)
int quiescence(Board& board, int alpha, int beta, int ply);
int negamax(Board& board, int depth, int alpha, int beta, int ply, std::vector<uint64_t>& history, std::vector<Move>& pvLine);

// movetimeMs > 0: time-limited, effectively unlimited depth (search until time runs out).
// movetimeMs <= 0: depth-limited, no time limit.
Move getBestMove(Board& board, int maxDepth, int movetimeMs = -1, const std::vector<uint64_t>& history = {}, int ply = 0);

// Request the current search to stop as soon as possible.
// Safe to call even if no search is running.
void request_stop_search();
void set_use_tt(bool enabled);
void clear_search_heuristics();


#endif
