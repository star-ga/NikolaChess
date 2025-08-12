
// Transposition table entry shared definition.
// Copyright (c) 2025 CPUTER Inc.
#pragma once
#include "board.h"

struct TTEntry {
    int depth;
    int score;
    int flag; // 0=EXACT,1=LOWERBOUND,2=UPPERBOUND
    nikola::Move bestMove;
};
