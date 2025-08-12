// GPU evaluation functions for NikolaChess.
//
// Copyright (c) 2025 CPUTER Inc.
// All rights reserved.  See the LICENSE file for license terms.
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
#include <cstring>
#include <algorithm>

namespace nikola {

// Number of CUDA streams to use for overlapping copy and compute.  Default is
// one stream which mimics the previous synchronous behaviour.  The value is
// configurable at runtime via setGpuStreams and the --gpu-streams command line
// option.
static int g_gpu_streams = 1;

void setGpuStreams(int n) {
    g_gpu_streams = (n > 0) ? n : 1;
}

// Host copies of mg piece tables and material values.  These mirror
// the definitions in board.cpp and are used to initialise the
// device tables.  We duplicate them here because evaluate.cu does
// not include board.cpp.  The values are taken from PeSTO's
// evaluation function【406358174428833†L70-L95】.
static const int hMaterialValues[6] = {100, 320, 330, 500, 900, 100000};
static const int h_mg_pawn_table[64] = {
     0,   0,   0,   0,   0,   0,  0,   0,
    98, 134,  61,  95,  68, 126, 34, -11,
     -6,   7,  26,  31,  65,  56, 25, -20,
    -14,  13,   6,  21,  23,  12, 17, -23,
    -27,  -2,  -5,  12,  17,   6, 10, -25,
    -26,  -4,  -4, -10,   3,   3, 33, -12,
    -35,  -1, -20, -23, -15,  24, 38, -22,
     0,   0,   0,   0,   0,   0,  0,   0
};
static const int h_mg_knight_table[64] = {
    -167, -89, -34, -49,  61, -97, -15, -107,
     -73, -41,  72,  36,  23,  62,   7,  -17,
     -47,  60,  37,  65,  84, 129,  73,   44,
      -9,  17,  19,  53,  37,  69,  18,   22,
     -13,   4,  16,  13,  28,  19,  21,   -8,
     -23,  -9,  12,  10,  19,  17,  25,  -16,
     -29, -53, -12,  -3,  -1,  18, -14,  -19,
    -105, -21, -58, -33, -17, -28, -19,  -23
};
static const int h_mg_bishop_table[64] = {
    -29,   4, -82, -37, -25, -42,   7,  -8,
    -26,  16, -18, -13,  30,  59,  18, -47,
    -16,  37,  43,  40,  35,  50,  37,  -2,
     -4,   5,  19,  50,  37,  37,   7,  -2,
     -6,  13,  13,  26,  34,  12,  10,   4,
      0,  15,  15,  15,  14,  27,  18,  10,
      4,  15,  16,   0,   7,  21,  33,   1,
    -33,  -3, -14, -21, -13, -12, -39, -21
};
static const int h_mg_rook_table[64] = {
     32,  42,  32,  51, 63,  9,  31,  43,
     27,  32,  58,  62, 80, 67,  26,  44,
     -5,  19,  26,  36, 17, 45,  61,  16,
    -24, -11,   7,  26, 24, 35,  -8, -20,
    -36, -26, -12,  -1,  9, -7,   6, -23,
    -45, -25, -16, -17,  3,  0,  -5, -33,
    -44, -16, -20,  -9, -1, 11,  -6, -71,
    -19, -13,   1,  17, 16,  7, -37, -26
};
static const int h_mg_queen_table[64] = {
    -28,   0,  29,  12,  59,  44,  43,  45,
    -24, -39,  -5,   1, -16,  57,  28,  54,
    -13, -17,   7,   8,  29,  56,  47,  57,
    -27, -27, -16, -16,  -1,  17,  -2,   1,
     -9, -26,  -9, -10,  -2,  -4,   3,  -3,
    -14,   2, -11,  -2,  -5,   2,  14,   5,
    -35,  -8,  11,   2,   8,  15,  -3,   1,
     -1, -18,  -9,  10, -15, -25, -31, -50
};
static const int h_mg_king_table[64] = {
    -65,  23,  16, -15, -56, -34,   2,  13,
     29,  -1, -20,  -7,  -8,  -4, -38, -29,
     -9,  24,   2, -16, -20,   6,  22, -22,
    -17, -20, -12, -27, -30, -25, -14, -36,
    -49,  -1, -27, -39, -46, -44, -33, -51,
    -14, -14, -22, -46, -44, -30, -15, -27,
      1,   7,  -8, -64, -43, -16,   9,   8,
    -15,  36,  12, -54,   8, -28,  24,  14
};

// Helper to flip a square index vertically.  Mirrors the board for
// Black’s perspective.
static inline int hFlipSq(int sq) {
    int rank = sq / 8;
    int file = sq % 8;
    return (7 - rank) * 8 + file;
}

// Device function to convert a piece code into a material score.  The
// values match those used by evaluateBoardCPU.  Positive scores
// favour white, negative favour black.
// Device table mapping piece codes (±1..±6) to material values.  This
// mirrors the materialValues array in the CPU evaluator.
__constant__ int dMaterialValues[6] = {100, 320, 330, 500, 900, 100000};

__device__ int devPieceValue(int8_t p) {
    if (p == 0) return 0;
    int idx = abs((int)p) - 1;
    int val = dMaterialValues[idx];
    return (p > 0 ? val : -val);
}

// Constant device array storing the middle‑game piece‑square table for
// both colours.  The layout is 12 contiguous blocks of 64 entries:
// index = colour * 6 + pieceType, where colour=0 for White and 1
// for Black and pieceType is 0..5 for pawn..king.  Each block of
// 64 ints represents the board squares in rank‑file order.  This
// table is filled by the host prior to kernel launch.
__constant__ int dMgTable[12 * 64];

// Constants for pawn structure and bishop pair heuristics.  These
// values mirror those used in the CPU evaluation.  They are copied
// from host arrays before kernel launch.  passedBonusByRank gives a
// bonus for passed pawns on each rank (0–7).  doubledPenalty and
// isolatedPenalty penalise doubled and isolated pawns.  bishopPairBonus
// rewards owning two bishops.
__constant__ int dPassedBonusByRank[8];
__constant__ int dDoubledPenalty;
__constant__ int dIsolatedPenalty;
__constant__ int dBishopPairBonus;

// Kernel to evaluate multiple boards in parallel.  Each thread
// computes the evaluation score for a single board and writes the
// result into the corresponding element of the outScores array.
__global__ void evalKernel(const Board* boards, int* outScores, int n) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;
    const Board& board = boards[idx];
    int score = 0;
    // Sum piece‑square values from the constant mg table.  We ignore
    // mobility on the GPU because generating legal moves on the GPU
    // would be expensive.  Each thread handles one board.
    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 8; ++c) {
            int8_t p = board.squares[r][c];
            if (p == 0) continue;
            int type = (p > 0 ? p - 1 : (-p - 1));
            int colour = (p > 0 ? 0 : 1);
            int idxTable = (colour * 6 + type) * 64 + (r * 8 + c);
            score += dMgTable[idxTable];
        }
    }
    outScores[idx] = score;
}

// Host wrapper to evaluate a batch of boards on the GPU.  It
// allocates device memory, copies boards to the GPU, launches a
// kernel with enough threads, and copies back the results.
std::vector<int> evaluateBoardsGPU(const Board* boards, int nBoards) {
    if (nBoards <= 0) return {};
    // Track CUDA errors so we can perform clean-up before throwing.
    cudaError_t err = cudaSuccess;
    size_t boardsBytes = sizeof(Board) * nBoards;
    size_t scoresBytes = sizeof(int) * nBoards;

    // Allocate pinned host buffers to enable asynchronous copies.
    Board* hBoardsPinned = nullptr;
    int* hScoresPinned = nullptr;
    err = cudaMallocHost((void**)&hBoardsPinned, boardsBytes);
    if (err != cudaSuccess) {
        throw std::runtime_error("cudaMallocHost for boards failed");
    }
    err = cudaMallocHost((void**)&hScoresPinned, scoresBytes);
    if (err != cudaSuccess) {
        cudaFreeHost(hBoardsPinned);
        throw std::runtime_error("cudaMallocHost for scores failed");
    }
    std::memcpy(hBoardsPinned, boards, boardsBytes);

    // Allocate device memory.
    Board* dBoards = nullptr;
    int* dScores = nullptr;
    err = cudaMalloc((void**)&dBoards, boardsBytes);
    if (err != cudaSuccess) {
        cudaFreeHost(hBoardsPinned);
        cudaFreeHost(hScoresPinned);
        throw std::runtime_error("cudaMalloc for boards failed");
    }
    err = cudaMalloc((void**)&dScores, scoresBytes);
    if (err != cudaSuccess) {
        cudaFree(dBoards);
        cudaFreeHost(hBoardsPinned);
        cudaFreeHost(hScoresPinned);
        throw std::runtime_error("cudaMalloc for scores failed");
    }

    // Initialise material and piece‑square tables on the device.  We
    // compute the 12×64 mg table on the host and copy it into constant
    // memory.  The mapping is: colour 0 (White) and 1 (Black) combined
    // with piece types 0..5 (pawn..king).  The mg table entry equals
    // material value plus square bonus for White; for Black the square
    // index is flipped vertically【406358174428833†L230-L240】.
    int hostMgTable[12 * 64];
    // Precompute mg table.
    const int* tables[6] = {h_mg_pawn_table, h_mg_knight_table, h_mg_bishop_table,
                            h_mg_rook_table, h_mg_queen_table, h_mg_king_table};
    for (int colour = 0; colour < 2; ++colour) {
        for (int p = 0; p < 6; ++p) {
            for (int sq = 0; sq < 64; ++sq) {
                int idx = (colour * 6 + p) * 64 + sq;
                int bonus;
                if (colour == 0) {
                    bonus = tables[p][sq];
                } else {
                    bonus = tables[p][hFlipSq(sq)];
                }
                hostMgTable[idx] = hMaterialValues[p] + bonus;
            }
        }
    }
    // Copy material values and mg table to constant memory.
    err = cudaMemcpyToSymbol(dMaterialValues, hMaterialValues, sizeof(hMaterialValues));
    if (err != cudaSuccess) {
        cudaFree(dBoards);
        cudaFree(dScores);
        cudaFreeHost(hBoardsPinned);
        cudaFreeHost(hScoresPinned);
        throw std::runtime_error("cudaMemcpyToSymbol for material values failed");
    }
    err = cudaMemcpyToSymbol(dMgTable, hostMgTable, sizeof(int) * 12 * 64);
    if (err != cudaSuccess) {
        cudaFree(dBoards);
        cudaFree(dScores);
        cudaFreeHost(hBoardsPinned);
        cudaFreeHost(hScoresPinned);
        throw std::runtime_error("cudaMemcpyToSymbol for mg table failed");
    }

    // Copy pawn structure and bishop pair constants to constant memory.
    // Host definitions mirror those in CPU evaluation.
    static const int passedBonusByRankHost[8] = {0, 10, 20, 30, 50, 80, 130, 0};
    int doubledPenaltyHost = 20;
    int isolatedPenaltyHost = 30;
    int bishopPairBonusHost = 50;
    err = cudaMemcpyToSymbol(dPassedBonusByRank, passedBonusByRankHost, sizeof(int) * 8);
    if (err != cudaSuccess) {
        cudaFree(dBoards);
        cudaFree(dScores);
        cudaFreeHost(hBoardsPinned);
        cudaFreeHost(hScoresPinned);
        throw std::runtime_error("cudaMemcpyToSymbol for passedBonusByRank failed");
    }
    err = cudaMemcpyToSymbol(dDoubledPenalty, &doubledPenaltyHost, sizeof(int));
    if (err != cudaSuccess) {
        cudaFree(dBoards);
        cudaFree(dScores);
        cudaFreeHost(hBoardsPinned);
        cudaFreeHost(hScoresPinned);
        throw std::runtime_error("cudaMemcpyToSymbol for doubledPenalty failed");
    }
    err = cudaMemcpyToSymbol(dIsolatedPenalty, &isolatedPenaltyHost, sizeof(int));
    if (err != cudaSuccess) {
        cudaFree(dBoards);
        cudaFree(dScores);
        cudaFreeHost(hBoardsPinned);
        cudaFreeHost(hScoresPinned);
        throw std::runtime_error("cudaMemcpyToSymbol for isolatedPenalty failed");
    }
    err = cudaMemcpyToSymbol(dBishopPairBonus, &bishopPairBonusHost, sizeof(int));
    if (err != cudaSuccess) {
        cudaFree(dBoards);
        cudaFree(dScores);
        cudaFreeHost(hBoardsPinned);
        cudaFreeHost(hScoresPinned);
        throw std::runtime_error("cudaMemcpyToSymbol for bishopPairBonus failed");
    }

    // Create CUDA streams and launch asynchronous copies and kernels.
    int streamCount = std::min(std::max(1, g_gpu_streams), nBoards);
    std::vector<cudaStream_t> streams(streamCount);
    for (int i = 0; i < streamCount; ++i) {
        err = cudaStreamCreate(&streams[i]);
        if (err != cudaSuccess) {
            for (int j = 0; j < i; ++j) cudaStreamDestroy(streams[j]);
            cudaFree(dBoards);
            cudaFree(dScores);
            cudaFreeHost(hBoardsPinned);
            cudaFreeHost(hScoresPinned);
            throw std::runtime_error("cudaStreamCreate failed");
        }
    }

    int boardsPerStream = (nBoards + streamCount - 1) / streamCount;
    for (int s = 0; s < streamCount; ++s) {
        int offset = s * boardsPerStream;
        int count = std::min(boardsPerStream, nBoards - offset);
        if (count <= 0) break;
        size_t bBytes = sizeof(Board) * count;
        size_t sBytes = sizeof(int) * count;
        err = cudaMemcpyAsync(dBoards + offset, hBoardsPinned + offset, bBytes,
                              cudaMemcpyHostToDevice, streams[s]);
        if (err != cudaSuccess) break;
        int blockSize = 128;
        int numBlocks = (count + blockSize - 1) / blockSize;
        evalKernel<<<numBlocks, blockSize, 0, streams[s]>>>(dBoards + offset,
                                                            dScores + offset,
                                                            count);
        // Capture any kernel launch failure immediately.
        err = cudaGetLastError();
        if (err != cudaSuccess) break;
        err = cudaMemcpyAsync(hScoresPinned + offset, dScores + offset, sBytes,
                              cudaMemcpyDeviceToHost, streams[s]);
        if (err != cudaSuccess) break;
    }

    if (err != cudaSuccess) {
        for (int s = 0; s < streamCount; ++s) cudaStreamDestroy(streams[s]);
        cudaFree(dBoards);
        cudaFree(dScores);
        cudaFreeHost(hBoardsPinned);
        cudaFreeHost(hScoresPinned);
        throw std::runtime_error("CUDA asynchronous launch failed");
    }

    for (int s = 0; s < streamCount; ++s) {
        cudaError_t syncErr = cudaStreamSynchronize(streams[s]);
        cudaStreamDestroy(streams[s]);
        if (syncErr != cudaSuccess) {
            cudaFree(dBoards);
            cudaFree(dScores);
            cudaFreeHost(hBoardsPinned);
            cudaFreeHost(hScoresPinned);
            throw std::runtime_error("CUDA stream execution failed");
        }
    }

    std::vector<int> scores(nBoards);
    std::memcpy(scores.data(), hScoresPinned, scoresBytes);

    cudaFree(dBoards);
    cudaFree(dScores);
    cudaFreeHost(hBoardsPinned);
    cudaFreeHost(hScoresPinned);
    return scores;
}

} // namespace nikola
