#pragma once
#include <vector>
#include "board.h"
#include "bitboard.h"

namespace nikola {

class NNUE {
public:
    NNUE(int inputSize = 12*64, int hidden1 = 256, int hidden2 = 32);
    int evaluate(const Bitboards& bb) const;
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

