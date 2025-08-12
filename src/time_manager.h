#pragma once
#include "board.h"

namespace nikola {

// Returns ms budget for this move, or -1 for infinite (no clocks).
int compute_time_budget(const Board& b, bool whiteToMove,
                        int wtime, int btime, int winc, int binc,
                        int movestogo, int overhead_ms, double safety=0.10);

}
