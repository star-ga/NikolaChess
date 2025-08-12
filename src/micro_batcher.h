// MicroBatcher for batched GPU evaluation.
//
// This header defines a class that collects board evaluation requests
// from multiple callers and processes them in batches.  When the
// number of queued boards reaches a specified maximum batch size or
// after a timeout, the batch is flushed to the GPU (or CPU fallback)
// via evaluateBoardsGPU().  Each call to submit() returns a
// std::future<int> that will contain the evaluation score once the
// batch is processed.  A background worker thread performs the
// batching and evaluation.

#pragma once

#include "board.h"

#include <vector>
#include <future>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <thread>

namespace nikola {

class MicroBatcher {
public:
    // Create a micro-batcher with the specified maximum batch size and
    // flush interval in milliseconds.  The batcher will flush when
    // either maxBatch boards have been submitted or flushMs
    // milliseconds have elapsed since the last flush.
    MicroBatcher(size_t maxBatch, int flushMs);
    ~MicroBatcher();

    // Submit a board for evaluation.  Returns a future that will
    // contain the evaluation score.  The returned future will block
    // until the batch is processed.
    std::future<int> submit(const Board& board);

    // Force immediate flushing of the current batch.  This method
    // notifies the worker thread to process any queued boards without
    // waiting for the batch to fill or for the timeout.  Typically
    // called before shutting down.
    void flush();

private:
    void worker();
    size_t maxBatch;
    int flushMs;
    std::mutex mtx;
    std::condition_variable cv;
    bool stopFlag;
    std::vector<Board> pendingBoards;
    std::vector<std::promise<int>> pendingPromises;
    std::thread workerThread;
    std::chrono::steady_clock::time_point lastFlush;
};

} // namespace nikola