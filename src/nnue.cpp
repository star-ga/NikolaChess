#include "nnue.h"
#include <cstdlib>
#include <cmath>

namespace nikola {

NNUE::NNUE(int inputSize, int hiddenSize)
    : inputSize_(inputSize), hiddenSize_(hiddenSize),
      w1_(hiddenSize * inputSize, 0.0f), b1_(hiddenSize, 0.0f),
      w2_(hiddenSize, 0.0f), b2_(0.0f) {
    unsigned int seed = 42u;
    auto randf = [&seed]() {
        seed = seed * 1664525u + 1013904223u;
        return (seed & 0xFFFF) / 65535.0f - 0.5f;
    };
    for (float& w : w1_) w = randf() * 0.01f;
    for (float& w : w2_) w = randf() * 0.01f;
    for (float& b : b1_) b = randf() * 0.1f;
    b2_ = randf() * 0.1f;
}

int NNUE::evaluate(const Bitboards& bb) const {
    std::vector<float> hidden(hiddenSize_, 0.0f);
    for (int h = 0; h < hiddenSize_; ++h) {
        float sum = b1_[h];
        for (int i = 0; i < inputSize_; ++i) {
            int piece = i / 64;
            int sq = i % 64;
            if (piece < 12 && bb_is_set(bb.pieces[piece], sq)) {
                sum += w1_[h * inputSize_ + i];
            }
        }
        hidden[h] = sum > 0 ? sum : 0.0f;
    }
    float out = b2_;
    for (int h = 0; h < hiddenSize_; ++h) out += w2_[h] * hidden[h];
    int score = static_cast<int>(out * 100);
    if (score > 100000) score = 100000;
    if (score < -100000) score = -100000;
    return score;
}

void NNUE::train(const std::vector<std::vector<float>>& inputs,
                 const std::vector<float>& targets,
                 int epochs, float lr) {
    for (int e = 0; e < epochs; ++e) {
        for (size_t n = 0; n < inputs.size(); ++n) {
            const auto& x = inputs[n];
            std::vector<float> hidden(hiddenSize_, 0.0f);
            for (int h = 0; h < hiddenSize_; ++h) {
                float sum = b1_[h];
                for (int i = 0; i < inputSize_; ++i) {
                    sum += w1_[h * inputSize_ + i] * x[i];
                }
                hidden[h] = sum > 0 ? sum : 0.0f;
            }
            float out = b2_;
            for (int h = 0; h < hiddenSize_; ++h) out += w2_[h] * hidden[h];
            float err = out - targets[n];
            for (int h = 0; h < hiddenSize_; ++h) {
                w2_[h] -= lr * err * hidden[h];
            }
            b2_ -= lr * err;
            for (int h = 0; h < hiddenSize_; ++h) {
                float gradHidden = err * w2_[h] * (hidden[h] > 0 ? 1.0f : 0.0f);
                for (int i = 0; i < inputSize_; ++i) {
                    w1_[h * inputSize_ + i] -= lr * gradHidden * x[i];
                }
                b1_[h] -= lr * gradHidden;
            }
        }
    }
}

int nnueEvaluate(const Board& board) {
    static NNUE net; // default network
    Bitboards bb = boardToBitboards(board);
    return net.evaluate(bb);
}

void nnueTrainBoards(const std::vector<Board>& boards, const std::vector<int>& targets,
                     int epochs, float lr) {
    NNUE net;
    std::vector<std::vector<float>> inputs;
    inputs.reserve(boards.size());
    std::vector<float> t;
    t.reserve(boards.size());
    for (size_t i = 0; i < boards.size(); ++i) {
        Bitboards bb = boardToBitboards(boards[i]);
        std::vector<float> in(12 * 64, 0.0f);
        for (int piece = 0; piece < 12; ++piece) {
            Bitboard b = bb.pieces[piece];
            while (b) {
                int sq = bb_lsb(b);
                bb_pop_lsb(b);
                int feature = piece * 64 + sq;
                if (feature < (int)in.size()) in[feature] = 1.0f;
            }
        }
        inputs.push_back(std::move(in));
        t.push_back(targets[i] / 100.0f);
    }
    net.train(inputs, t, epochs, lr);
}

} // namespace nikola

