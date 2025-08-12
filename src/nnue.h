#pragma once
#include <vector>
#include "board.h"
#include "bitboard.h"

namespace nikola {

class NNUE {
public:
    NNUE(int inputSize = 12*64, int hiddenSize = 128);
    int evaluate(const Bitboards& bb) const;
    void train(const std::vector<std::vector<float>>& inputs,
               const std::vector<float>& targets,
               int epochs, float lr);
private:
    int inputSize_;
    int hiddenSize_;
    std::vector<float> w1_;
    std::vector<float> b1_;
    std::vector<float> w2_;
    float b2_;
};

int nnueEvaluate(const Board& board);
void nnueTrainBoards(const std::vector<Board>& boards,
                     const std::vector<int>& targets,
                     int epochs, float lr);

} // namespace nikola

