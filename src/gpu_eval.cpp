// GPU evaluation service implementation for NikolaChess.
//
// When NIKOLA_USE_TENSORRT is enabled this file provides a minimal
// placeholder that could be extended to run NNUE inference via
// TensorRT.  In the default build a stub implementation is used so the
// project can compile without the TensorRT SDK installed.

#include "gpu_eval.h"
#include <future>
#include <vector>

#ifdef NIKOLA_USE_TENSORRT
#include <NvInfer.h>
#include <cuda_runtime.h>
#include <iostream>

namespace nikola {
namespace {
// Simple TensorRT logger that prints warnings and errors to stdout.
class TrtLogger : public nvinfer1::ILogger {
public:
    void log(Severity severity, const char* msg) noexcept override {
        if (severity <= Severity::kWARNING) {
            std::cout << "[TensorRT] " << msg << std::endl;
        }
    }
} gLogger;
} // namespace

void GpuEval::init(const std::string& /*model_path*/,
                   const std::string& /*precision*/,
                   int /*device_id*/,
                   size_t /*max_batch*/,
                   int /*streams*/) {
    // A real implementation would deserialize a TensorRT engine and
    // create execution contexts here.  For now we simply act as a
    // stub so the rest of the program can link successfully.
}

std::future<float> GpuEval::submit(const float* /*features*/, size_t /*len*/) {
    std::promise<float> p;
    // TODO: enqueue features for TensorRT execution and return future.
    p.set_value(0.0f);
    return p.get_future();
}

void GpuEval::flush() {
    // In a full implementation this would ensure outstanding batches
    // are processed.  Nothing to do in the stub.
}

} // namespace nikola

#else // !NIKOLA_USE_TENSORRT

// ---------------------------------------------------------------------------
// Fallback stub used when TensorRT support is disabled.  This mirrors the
// previous behaviour and keeps the rest of the engine unchanged.
// ---------------------------------------------------------------------------

// Simple CPU thread-pool implementation that mirrors a production TensorRT
// backend.  Tasks consisting of NNUE feature vectors are queued and processed
// asynchronously by a fixed number of worker threads.  This keeps the public
// API intact while providing an actual evaluation pipeline.

#include "nnue.h"
#include "bitboard.h"

#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>

namespace nikola {
namespace {

struct Task {
    std::vector<float> features;
    std::promise<float> promise;
};

std::vector<std::thread> workers;
std::queue<Task> tasks;
std::mutex mtx;
std::condition_variable cv;
bool stopWorkers = false;
NNUE gNet; // default network instance
size_t g_maxBatch = 1;
size_t inFlight = 0;

void workerLoop() {
    while (true) {
        std::vector<Task> batch;
        {
            std::unique_lock<std::mutex> lock(mtx);
            cv.wait(lock, [] { return stopWorkers || !tasks.empty(); });
            if (stopWorkers && tasks.empty()) return;
            size_t n = std::min(g_maxBatch, tasks.size());
            inFlight += n;
            for (size_t i = 0; i < n; ++i) {
                batch.push_back(std::move(tasks.front()));
                tasks.pop();
            }
        }
        for (auto& t : batch) {
            Bitboards bb{};
            for (int piece = 0; piece < 12; ++piece) {
                for (int sq = 0; sq < 64; ++sq) {
                    if (t.features[piece * 64 + sq] > 0.5f)
                        bb_set(bb.pieces[piece], sq);
                }
            }
            float score = static_cast<float>(gNet.evaluate(bb));
            t.promise.set_value(score);
        }
        {
            std::lock_guard<std::mutex> lock(mtx);
            inFlight -= batch.size();
            if (tasks.empty() && inFlight == 0) cv.notify_all();
        }
    }
}

void shutdown() {
    {
        std::lock_guard<std::mutex> lock(mtx);
        stopWorkers = true;
    }
    cv.notify_all();
    for (auto& th : workers) {
        if (th.joinable()) th.join();
    }
    workers.clear();
}

struct Shutdown { ~Shutdown() { shutdown(); } } shutdownGuard;

} // namespace

void GpuEval::init(const std::string& /*model_path*/,
                   const std::string& /*precision*/,
                   int /*device_id*/,
                   size_t max_batch,
                   int streams) {
    shutdown();
    {
        std::lock_guard<std::mutex> lock(mtx);
        stopWorkers = false;
    }
    g_maxBatch = std::max<size_t>(1, max_batch);
    for (int i = 0; i < streams; ++i) {
        workers.emplace_back(workerLoop);
    }
}

std::future<float> GpuEval::submit(const float* features, size_t len) {
    Task t;
    t.features.assign(features, features + len);
    std::future<float> fut = t.promise.get_future();
    {
        std::lock_guard<std::mutex> lock(mtx);
        tasks.push(std::move(t));
    }
    cv.notify_one();
    return fut;
}

void GpuEval::flush() {
    std::unique_lock<std::mutex> lock(mtx);
    cv.wait(lock, [] { return tasks.empty() && inFlight == 0; });
}

} // namespace nikola

#endif // NIKOLA_USE_TENSORRT

