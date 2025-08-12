#include "board.h"
#include "gpu_eval.h"
#include <future>
#include <mutex>
#include <vector>

namespace nikola {

// Number of logical "streams" for the CPU-based GPU simulator.
static int g_streams = 1;
static std::once_flag initFlag;

// Convert a Board into a 12*64 feature vector used by the NNUE evaluator.
static void boardToFeatures(const Board& b, std::vector<float>& out) {
    out.assign(12 * 64, 0.0f);
    Bitboards bb = boardToBitboards(b);
    for (int piece = 0; piece < 12; ++piece) {
        Bitboard bits = bb.pieces[piece];
        while (bits) {
            int sq = bb_lsb(bits);
            bb_pop_lsb(bits);
            out[piece * 64 + sq] = 1.0f;
        }
    }
}

std::vector<int> evaluateBoardsGPU(const Board* boards, int nBoards) {
    std::call_once(initFlag, [] { GpuEval::init("", "fp32", 0, 0, g_streams); });
    std::vector<int> scores;
    scores.reserve(nBoards);
    std::vector<std::vector<float>> feats(nBoards);
    std::vector<std::future<float>> futs;
    futs.reserve(nBoards);
    for (int i = 0; i < nBoards; ++i) {
        boardToFeatures(boards[i], feats[i]);
        futs.push_back(GpuEval::submit(feats[i].data(), feats[i].size()));
    }
    GpuEval::flush();
    for (auto& f : futs) {
        scores.push_back(static_cast<int>(f.get()));
    }
    return scores;
}

void setGpuStreams(int n) {
    g_streams = (n > 0) ? n : 1;
    GpuEval::init("", "fp32", 0, 0, g_streams);
}

} // namespace nikola
