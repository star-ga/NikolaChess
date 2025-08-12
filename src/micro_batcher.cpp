// Implementation of MicroBatcher for batched GPU evaluation.
//
// This file defines the logic for the MicroBatcher class declared
// in micro_batcher.h.  The batcher collects boards submitted by
// multiple callers and periodically flushes them to the GPU for
// evaluation.  If GPU evaluation fails or is unavailable, the
// batcher falls back to CPU evaluation.  Each submit() returns a
// future that will eventually hold the evaluation score.

#include "micro_batcher.h"

#include <cassert>
#include <cstdlib>
#include <iostream>

namespace nikola {

// Forward declarations of evaluation functions.  These functions are
// defined elsewhere in the engine.  evaluateBoardsGPU returns a
// vector of scores for nBoards boards and may throw on error.  If
// nBoards is zero it returns an empty vector.  evaluateBoardCPU
// returns the static evaluation of a single board.
std::vector<int> evaluateBoardsGPU(const Board* boards, int nBoards);
int evaluateBoardCPU(const Board& board);

MicroBatcher::MicroBatcher(size_t maxBatch_, int flushMs_)
    : maxBatch(maxBatch_), flushMs(flushMs_), stopFlag(false), lastFlush(std::chrono::steady_clock::now()) {
    // Spawn the worker thread that handles batching and evaluation.
    workerThread = std::thread(&MicroBatcher::worker, this);
}

MicroBatcher::~MicroBatcher() {
    // Signal the worker to stop and wake it up.  We also flush any
    // pending boards before shutting down.
    {
        std::unique_lock<std::mutex> lock(mtx);
        stopFlag = true;
        cv.notify_all();
    }
    if (workerThread.joinable()) {
        workerThread.join();
    }
    // After worker joins, there should be no pending boards or
    // promises left.  Any remaining promises would indicate logic
    // errors.  We simply ignore them here since there's no way to
    // fulfil them without a running worker.
}

std::future<int> MicroBatcher::submit(const Board& board) {
    // Create a promise/future pair for this evaluation.  We create
    // the promise here and store it in the pending list; the future
    // will be returned to the caller.  The promise will be fulfilled
    // when the worker processes the batch.
    std::promise<int> promise;
    std::future<int> fut = promise.get_future();
    {
        std::unique_lock<std::mutex> lock(mtx);
        pendingBoards.push_back(board);
        pendingPromises.push_back(std::move(promise));
        // If the batch size has reached the maximum, notify the
        // worker immediately to trigger a flush.
        if (pendingBoards.size() >= maxBatch) {
            cv.notify_all();
        }
    }
    return fut;
}

void MicroBatcher::flush() {
    // Wake up the worker and reset the last flush time.  The worker
    // will see that there are pending boards and will process them
    // immediately.
    {
        std::unique_lock<std::mutex> lock(mtx);
        // Adjust lastFlush backwards so that the next iteration will
        // satisfy the time‑based flushing condition.
        lastFlush = std::chrono::steady_clock::now() - std::chrono::milliseconds(flushMs);
        cv.notify_all();
    }
}

void MicroBatcher::worker() {
    std::vector<Board> boards;
    std::vector<std::promise<int>> promises;
    while (true) {
        boards.clear();
        promises.clear();
        {
            std::unique_lock<std::mutex> lock(mtx);
            // Wait until there are boards to process, the flush
            // interval has elapsed, or a stop request arrives.
            while (!stopFlag) {
                auto now = std::chrono::steady_clock::now();
                // Determine how long we have waited since last flush.
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastFlush).count();
                bool timeExpired = (elapsed >= flushMs);
                bool haveBatch = (!pendingBoards.empty());
                bool sizeReached = (pendingBoards.size() >= maxBatch);
                // If we have a batch ready based on size or time, break out.
                if ((haveBatch && timeExpired) || sizeReached) {
                    break;
                }
                // Otherwise, wait for the remaining time until flush or
                // until new submissions or stop.  We cap the wait time
                // at flushMs to avoid oversleeping.
                auto waitTime = std::chrono::milliseconds(flushMs);
                if (haveBatch) {
                    // We have at least one board; wait only until the
                    // flush interval expires.
                    waitTime = std::chrono::milliseconds(flushMs - static_cast<int>(elapsed));
                }
                cv.wait_for(lock, waitTime);
                // Loop and recheck conditions.
            }
            // If we're being stopped and there are no pending boards,
            // exit the worker.
            if (stopFlag && pendingBoards.empty()) {
                return;
            }
            // Swap pending lists into local variables to process.
            boards.swap(pendingBoards);
            promises.swap(pendingPromises);
            lastFlush = std::chrono::steady_clock::now();
        } // Unlock the mutex while evaluating.

        // Evaluate the batch.  First attempt GPU evaluation.
        std::vector<int> scores;
        bool gpuOk = false;
        try {
            if (!boards.empty()) {
                scores = evaluateBoardsGPU(boards.data(), static_cast<int>(boards.size()));
                gpuOk = (scores.size() == boards.size());
            }
        } catch (...) {
            gpuOk = false;
        }
        if (!gpuOk) {
            // GPU evaluation failed or returned wrong size; fall back to
            // CPU evaluation.  Reserve space and compute each board.
            scores.resize(boards.size());
            for (size_t i = 0; i < boards.size(); ++i) {
                scores[i] = evaluateBoardCPU(boards[i]);
            }
        }
        // Fulfil promises with the computed scores.  If there are
        // fewer scores than promises (which should not happen), we
        // deliver zero for the missing ones to avoid blocking the
        // caller forever.
        for (size_t i = 0; i < promises.size(); ++i) {
            int val = (i < scores.size()) ? scores[i] : 0;
            promises[i].set_value(val);
        }
        // Loop back to wait for the next batch or shutdown.
    }
}

} // namespace nikola