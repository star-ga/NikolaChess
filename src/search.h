#pragma once
#include "board.h"

namespace nikola {

Move findBestMove(const Board& board, int depth, int timeLimitMs = 0);
void setUseGpu(bool use);

}

