#pragma once
#include <vector>
#include "board.h"
#include "bitboard.h"

namespace nikola {

class NNUE {
public:
    // Include an extra input feature for side to move.
    // Positive value represents white to move, negative black to move.
    NNUE(int inputSize = 12*64 + 1, int hidden1 = 256, int hidden2 = 32);
    // Evaluate the given bitboards with the supplied side to move flag.
    int evaluate(const Bitboards& bb, bool whiteToMove) const;
    void train(const std::vector<std::vector<float>>& inputs,
               const std::vector<float>& targets,
               int epochs, float lr);
private:
    int inputSize_;
    int hidden1_;
    int hidden2_;
    std::vector<float> w1_;
    std::vector<float> b1_;
    std::vector<float> w2_;
    std::vector<float> b2_;
    std::vector<float> w3_;
    float b3_;
};

int nnueEvaluate(const Board& board);
void nnueTrainBoards(const std::vector<Board>& boards,
                     const std::vector<int>& targets,
                     int epochs, float lr);

} // namespace nikola

