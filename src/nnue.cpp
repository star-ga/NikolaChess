#include "nnue.h"
#include <cstdlib>
#include <cmath>

namespace nikola {

NNUE::NNUE(int inputSize, int hidden1, int hidden2)
    : inputSize_(inputSize), hidden1_(hidden1), hidden2_(hidden2),
      w1_(hidden1 * inputSize, 0.0f), b1_(hidden1, 0.0f),
      w2_(hidden2 * hidden1, 0.0f), b2_(hidden2, 0.0f),
      w3_(hidden2, 0.0f), b3_(0.0f) {
    unsigned int seed = 42u;
    auto randf = [&seed]() {
        seed = seed * 1664525u + 1013904223u;
        return (seed & 0xFFFF) / 65535.0f - 0.5f;
    };
    for (float& w : w1_) w = randf() * 0.01f;
    for (float& w : w2_) w = randf() * 0.01f;
    for (float& w : w3_) w = randf() * 0.01f;
    for (float& b : b1_) b = randf() * 0.1f;
    for (float& b : b2_) b = randf() * 0.1f;
    b3_ = randf() * 0.1f;
}

int NNUE::evaluate(const Bitboards& bb) const {
    std::vector<float> h1(hidden1_, 0.0f);
    for (int h = 0; h < hidden1_; ++h) {
        float sum = b1_[h];
        for (int i = 0; i < inputSize_; ++i) {
            int piece = i / 64;
            int sq = i % 64;
            if (piece < 12 && bb_is_set(bb.pieces[piece], sq)) {
                sum += w1_[h * inputSize_ + i];
            }
        }
        h1[h] = sum > 0 ? sum : 0.0f;
    }
    std::vector<float> h2(hidden2_, 0.0f);
    for (int h = 0; h < hidden2_; ++h) {
        float sum = b2_[h];
        for (int i = 0; i < hidden1_; ++i) {
            sum += w2_[h * hidden1_ + i] * h1[i];
        }
        h2[h] = sum > 0 ? sum : 0.0f;
    }
    float out = b3_;
    for (int h = 0; h < hidden2_; ++h) out += w3_[h] * h2[h];
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
            std::vector<float> h1(hidden1_, 0.0f);
            for (int h = 0; h < hidden1_; ++h) {
                float sum = b1_[h];
                for (int i = 0; i < inputSize_; ++i) {
                    sum += w1_[h * inputSize_ + i] * x[i];
                }
                h1[h] = sum > 0 ? sum : 0.0f;
            }
            std::vector<float> h2(hidden2_, 0.0f);
            for (int h = 0; h < hidden2_; ++h) {
                float sum = b2_[h];
                for (int i = 0; i < hidden1_; ++i) {
                    sum += w2_[h * hidden1_ + i] * h1[i];
                }
                h2[h] = sum > 0 ? sum : 0.0f;
            }
            float out = b3_;
            for (int h = 0; h < hidden2_; ++h) out += w3_[h] * h2[h];
            float err = out - targets[n];
            for (int h = 0; h < hidden2_; ++h) {
                w3_[h] -= lr * err * h2[h];
            }
            b3_ -= lr * err;
            std::vector<float> gradH2(hidden2_, 0.0f);
            for (int h = 0; h < hidden2_; ++h) {
                gradH2[h] = err * w3_[h] * (h2[h] > 0 ? 1.0f : 0.0f);
                for (int i = 0; i < hidden1_; ++i) {
                    w2_[h * hidden1_ + i] -= lr * gradH2[h] * h1[i];
                }
                b2_[h] -= lr * gradH2[h];
            }
            std::vector<float> gradH1(hidden1_, 0.0f);
            for (int i = 0; i < hidden1_; ++i) {
                float sum = 0.0f;
                for (int h = 0; h < hidden2_; ++h) {
                    sum += gradH2[h] * w2_[h * hidden1_ + i];
                }
                gradH1[i] = sum * (h1[i] > 0 ? 1.0f : 0.0f);
                for (int j = 0; j < inputSize_; ++j) {
                    w1_[i * inputSize_ + j] -= lr * gradH1[i] * x[j];
                }
                b1_[i] -= lr * gradH1[i];
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

