#include "board.h"
#include <vector>

namespace nikola {

// CPU fallback implementation of evaluateBoardsGPU. When CUDA is not
// available this function evaluates each board on the CPU using the
// existing evaluateBoardCPU helper.
std::vector<int> evaluateBoardsGPU(const Board* boards, int nBoards) {
    std::vector<int> scores;
    scores.reserve(nBoards);
    for (int i = 0; i < nBoards; ++i) {
        scores.push_back(evaluateBoardCPU(boards[i]));
    }
    return scores;
}

} // namespace nikola
