#include "board.h"
#include "bitboard.h"
#include "search.h"
#include "evaluation.h"
#include "uci.h"
#include <iostream>
#include <string>
#include <sstream>
#include <chrono>
#include <vector>
#include <algorithm>
#include <thread>
#include <atomic>

int main(int argc, char* argv[]) {
    std::cout.setf(std::ios::unitbuf); // Disable output buffering
    init_all();
    initLMRtables();
    handle_uci_commands(argc, argv);
}