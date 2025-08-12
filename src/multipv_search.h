#pragma once
#include <vector>
#include <utility>
#include <string>
#include "board.h"

namespace nikola {

struct RootResult {
    int scoreCentipawns; // use mate scores as large cp (e.g., 30000)
    Move firstMove;      // root move for this slot
    std::vector<Move> pv;// full PV moves
};

// Run MultiPV root search with aspiration + panic, respecting time budget.
// If depthCap <= 0, auto‑depth. Returns up to N results ordered best‑first.
std::vector<RootResult> search_multipv(Board root,
                                       int N,
                                       int depthCap,
                                       int timeBudgetMs);

}
