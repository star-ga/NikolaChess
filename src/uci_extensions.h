#pragma once
#include <vector>
#include <string>
#include "board.h"

namespace nikola {

void uci_print_id_and_options();
void uci_isready();
void uci_setoption(const std::vector<std::string>& tokens);

// Handles `go ...`: parses clocks/depth, computes budget, runs MultiPV, prints
// `info multipv N score cp|mate ... pv <moves>` lines and `bestmove`.
void uci_go(const Board& current, const std::vector<std::string>& tokens);

}
