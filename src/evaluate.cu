// GPU evaluation functions for NikolaChess.
//
// The purpose of this module is to offload the static evaluation of
// chess positions to the GPU.  Each CUDA thread evaluates one board
// independently.  Although evaluating a single position on the GPU is
// slower than on the CPU due to kernel launch overhead, the ability
// to evaluate large batches of positions concurrently makes this
// approach attractive when performing search in parallel.  The code
// here mirrors the material and simple positional heuristics used in
// the CPU evaluation for consistency.

#include "board.h"

#include <cuda_runtime.h>
#include <vector>
#include <stdexcept>

namespace nikola {

// Device function to convert a piece code into a material score.  The
// values match those used by evaluateBoardCPU.  Positive scores
// favour white, negative favour black.
__device__ int devPieceValue(int8_t p) {
    switch (p) {
        case WP: return 100;
        case WN: return 320;
        case WB: return 330;
        case WR: return 500;
        case WQ: return 900;
        case WK: return 100000;
        case BP: return -100;
        case BN: return -320;
        case BB: return -330;
        case BR: return -500;
        case BQ: return -900;
        case BK: return -100000;
        default: return 0;
    }
}

// Kernel to evaluate multiple boards in parallel.  Each thread
// computes the evaluation score for a single board and writes the
// result into the corresponding element of the outScores array.
__global__ void evalKernel(const Board* boards, int* outScores, int n) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;
    const Board& board = boards[idx];
    int score = 0;
    // Iterate through all squares.
    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 8; ++c) {
            int8_t piece = board.squares[r][c];
            score += devPieceValue(piece);
            // Simple positional bonuses similar to CPU evaluation.
            if (piece == WP && (r == 3 || r == 4) && (c >= 2 && c <= 5)) {
                score += 10;
            } else if (piece == BP && (r == 4 || r == 3) && (c >= 2 && c <= 5)) {
                score -= 10;
            } else if ((piece == WN || piece == BN) && (r >= 2 && r <= 5) && (c >= 2 && c <= 5)) {
                score += (piece > 0 ? 30 : -30);
            }
        }
    }
    outScores[idx] = score;
}

// Host wrapper to evaluate a batch of boards on the GPU.  It
// allocates device memory, copies boards to the GPU, launches a
// kernel with enough threads, and copies back the results.
std::vector<int> evaluateBoardsGPU(const Board* boards, int nBoards) {
    if (nBoards <= 0) return {};
    Board* dBoards = nullptr;
    int* dScores = nullptr;
    size_t boardsBytes = sizeof(Board) * nBoards;
    size_t scoresBytes = sizeof(int) * nBoards;
    cudaError_t err;
    err = cudaMalloc((void**)&dBoards, boardsBytes);
    if (err != cudaSuccess) {
        throw std::runtime_error("cudaMalloc for boards failed");
    }
    err = cudaMalloc((void**)&dScores, scoresBytes);
    if (err != cudaSuccess) {
        cudaFree(dBoards);
        throw std::runtime_error("cudaMalloc for scores failed");
    }
    // Copy boards to device
    err = cudaMemcpy(dBoards, boards, boardsBytes, cudaMemcpyHostToDevice);
    if (err != cudaSuccess) {
        cudaFree(dBoards);
        cudaFree(dScores);
        throw std::runtime_error("cudaMemcpy to device failed");
    }
    // Launch kernel with one thread per board.
    int blockSize = 128;
    int numBlocks = (nBoards + blockSize - 1) / blockSize;
    evalKernel<<<numBlocks, blockSize>>>(dBoards, dScores, nBoards);
    err = cudaGetLastError();
    if (err != cudaSuccess) {
        cudaFree(dBoards);
        cudaFree(dScores);
        throw std::runtime_error("Kernel launch failed");
    }
    // Copy results back to host.
    std::vector<int> scores(nBoards);
    err = cudaMemcpy(scores.data(), dScores, scoresBytes, cudaMemcpyDeviceToHost);
    if (err != cudaSuccess) {
        cudaFree(dBoards);
        cudaFree(dScores);
        throw std::runtime_error("cudaMemcpy from device failed");
    }
    cudaFree(dBoards);
    cudaFree(dScores);
    return scores;
}

} // namespace nikola
