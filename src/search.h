#pragma once
#include "board.h"

namespace nikola {

Move findBestMove(const Board& board, int depth, int timeLimitMs = 0);
void setUseGpu(bool use);

// Global MultiPV setting controlled via UCI option
extern int g_multiPV;

}

