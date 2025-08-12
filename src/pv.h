#pragma once
#include <vector>
#include <string>
#include "board.h"

namespace nikola {

// Extract PV via TT chaining starting from root board with first move already chosen.
std::vector<Move> extract_pv(const Board& root, int maxLen=60);

// Turn a Move into UCI string like e2e4 (promotion handled as e7e8q).
std::string move_to_uci(const Board& b, const Move& m);

} // namespace nikola
