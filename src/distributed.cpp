// Distributed search implementation for Supercomputer Chess Engine.
//
// Copyright (c) 2025 CPUTER Inc.
// All rights reserved.  See the LICENSE file for license terms.
//
// This file implements a small master/worker search prototype.  When
// compiled with MPI support the master (rank 0) generates all legal
// root moves for the current position and assigns them to worker
// ranks via `MPI_Send`.  Each worker evaluates the resulting child
// position and sends the score back with `MPI_Recv`.  The master keeps
// track of the best result and performs simple load balancing by
// dispatching a new root move to a worker as soon as it returns a
// score.  A dummy transposition‑table (TT) block is broadcast to all
// ranks to mimic sharing common search state.  When NCCL support is
// enabled, a communicator is created to illustrate how multi‑GPU
// broadcasts of the TT block could be performed.

#include "distributed.h"
#include "board.h"
#include "tt_entry.h"

#include <algorithm>
#include <deque>
#include <iostream>
#include <limits>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

// If MPI support is enabled, include the MPI header.  Users can
// define NIKOLA_USE_MPI in the build system to enable this path.
#ifdef NIKOLA_USE_MPI
#include <mpi.h>
#endif

// Optional NCCL support for intra‑node GPU communication.  When the
// NIKOLA_USE_NCCL preprocessor definition is provided a communicator is
// initialised so that the broadcasted TT block is mirrored across GPUs.
#ifdef NIKOLA_USE_NCCL
#include <nccl.h>
#include <cuda_runtime.h>
#endif

namespace nikola {

// Result payload exchanged between master and workers.  Each worker sends its
// evaluated score together with the originating move back to rank 0.
struct Result {
    int score;
    Move move;
};

int distributed_search() {
#ifdef NIKOLA_USE_MPI
    int provided;
    // Initialise MPI with support for multiple threads if available.
    MPI_Init_thread(nullptr, nullptr, MPI_THREAD_SERIALIZED, &provided);

    int rank = 0;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    int size = 1;
    MPI_Comm_size(MPI_COMM_WORLD, &size);

#ifdef NIKOLA_USE_NCCL
    // Set up an NCCL communicator when requested.  The unique ID is
    // generated on rank 0 and broadcast to all other ranks using MPI.
    ncclComm_t ncclComm;
    ncclUniqueId ncclId;
    if (rank == 0) {
        ncclGetUniqueId(&ncclId);
    }
    MPI_Bcast(&ncclId, sizeof(ncclId), MPI_BYTE, 0, MPI_COMM_WORLD);
    int deviceCount = 0;
    cudaGetDeviceCount(&deviceCount);
    cudaSetDevice(deviceCount ? rank % deviceCount : 0);
    ncclCommInitRank(&ncclComm, size, ncclId, rank);
#endif

    // Rank 0 starts from the standard initial position and broadcasts it
    // to all workers.
    Board root;
    if (rank == 0) {
        root = initBoard();
    }
    MPI_Bcast(&root, sizeof(Board), MPI_BYTE, 0, MPI_COMM_WORLD);

    // Share a dummy TT block so all ranks see the same base state.  A real
    // engine would share portions of the transposition table.
    std::vector<char> ttBlock(1024);
    MPI_Bcast(ttBlock.data(), static_cast<int>(ttBlock.size()), MPI_BYTE, 0,
              MPI_COMM_WORLD);
    if (rank < static_cast<int>(ttBlock.size()))
        ttBlock[rank] = 1;

#ifdef NIKOLA_USE_NCCL
    // Mirror the block to GPUs as an example of NCCL usage.  The block is
    // placed on each device and broadcast via NCCL so every GPU receives
    // identical data.
    char* dBlock = nullptr;
    cudaMalloc(&dBlock, ttBlock.size());
    cudaMemcpy(dBlock, ttBlock.data(), ttBlock.size(), cudaMemcpyHostToDevice);
    ncclBroadcast(dBlock, dBlock, ttBlock.size(), ncclChar, 0, ncclComm, 0);
    cudaFree(dBlock);
#endif

    if (rank == 0) {
        // Master: generate root moves and distribute them to workers.
        auto moves = generateMoves(root);
        int nextIndex = 0;

        // Send an initial move to each worker.
        for (int worker = 1; worker < size && nextIndex < (int)moves.size();
             ++worker) {
            MPI_Send(&moves[nextIndex], sizeof(Move), MPI_BYTE, worker, 0,
                     MPI_COMM_WORLD);
            ++nextIndex;
        }

        int active = std::min(size - 1, static_cast<int>(moves.size()));
        int bestScore =
            root.whiteToMove ? std::numeric_limits<int>::min()
                             : std::numeric_limits<int>::max();
        Move bestMove{};

        while (active > 0) {
            Result res;
            MPI_Status st;
            MPI_Recv(&res, sizeof(Result), MPI_BYTE, MPI_ANY_SOURCE, 1,
                     MPI_COMM_WORLD, &st);

            if (root.whiteToMove) {
                if (res.score > bestScore) {
                    bestScore = res.score;
                    bestMove = res.move;
                }
            } else {
                if (res.score < bestScore) {
                    bestScore = res.score;
                    bestMove = res.move;
                }
            }

            // Send another move to this worker if any remain.
            if (nextIndex < (int)moves.size()) {
                MPI_Send(&moves[nextIndex], sizeof(Move), MPI_BYTE, st.MPI_SOURCE,
                         0, MPI_COMM_WORLD);
                ++nextIndex;
            } else {
                // No more work: send termination message.
                Move term{};
                term.fromRow = -1;
                MPI_Send(&term, sizeof(Move), MPI_BYTE, st.MPI_SOURCE, 0,
                         MPI_COMM_WORLD);
                --active;
            }
        }

        std::cout << "Distributed best move score: " << bestScore << std::endl;
        MPI_Allreduce(MPI_IN_PLACE, ttBlock.data(), static_cast<int>(ttBlock.size()),
                      MPI_BYTE, MPI_BOR, MPI_COMM_WORLD);
        if (rank == 0) {
            int merged = 0;
            for (char c : ttBlock) if (c) ++merged;
            std::cout << "Merged TT entries: " << merged << std::endl;
        }
#ifdef NIKOLA_USE_NCCL
        ncclCommDestroy(ncclComm);
#endif
        MPI_Finalize();
        return 0;
    } else {
        // Worker: receive moves, evaluate and send back scores.
        while (true) {
            Move m;
            MPI_Status st;
            MPI_Recv(&m, sizeof(Move), MPI_BYTE, 0, 0, MPI_COMM_WORLD, &st);
            if (m.fromRow == -1) break; // termination signal

            Board child = makeMove(root, m);
            int score = evaluateBoardCPU(child);
            Result res{score, m};
            MPI_Send(&res, sizeof(Result), MPI_BYTE, 0, 1, MPI_COMM_WORLD);
        }
        MPI_Allreduce(MPI_IN_PLACE, ttBlock.data(), static_cast<int>(ttBlock.size()),
                      MPI_BYTE, MPI_BOR, MPI_COMM_WORLD);
#ifdef NIKOLA_USE_NCCL
        ncclCommDestroy(ncclComm);
#endif
        MPI_Finalize();
        return 0;
    }
#else
    // MPI is not enabled; run a local multi-threaded search that uses a shared
    // work queue (work stealing) and a simple transposition table shared across
    // threads.  This demonstrates the algorithms used in the distributed
    // version without requiring MPI.
    Board root = initBoard();
    auto moves = generateMoves(root);

    std::deque<Move> work(moves.begin(), moves.end());
    std::mutex workMutex;
    std::mutex ttMutex;
    std::unordered_map<std::string, TTEntry> tt;

    auto worker = [&]() {
        while (true) {
            Move m;
            {
                std::lock_guard<std::mutex> lock(workMutex);
                if (work.empty()) break;
                m = work.front();
                work.pop_front();
            }
            Board child = makeMove(root, m);
            std::string key = boardToFEN(child);
            {
                std::lock_guard<std::mutex> lock(ttMutex);
                if (tt.find(key) != tt.end()) continue; // TT hit
            }
            int score = evaluateBoardCPU(child);
            {
                std::lock_guard<std::mutex> lock(ttMutex);
                tt[key] = TTEntry{1, score, 0, m};
            }
        }
    };

    unsigned nThreads = std::max(1u, std::thread::hardware_concurrency());
    std::vector<std::thread> threads;
    threads.reserve(nThreads);
    for (unsigned i = 0; i < nThreads; ++i) {
        threads.emplace_back(worker);
    }
    for (auto& t : threads) t.join();
    return 0;
#endif
}

} // namespace nikola
