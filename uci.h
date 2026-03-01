#ifndef UCI_H
#define UCI_H

#include "board.h"
#include <string>

void bench();
extern int handle_uci_commands(int argc, char* argv[]);
extern std::string move_to_uci(const Move m);
#endif 