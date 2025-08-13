// GPU evaluation service implementation for NikolaChess.
//
// When NIKOLA_USE_TENSORRT is enabled this file provides a minimal
// placeholder that could be extended to run NNUE inference via
// TensorRT.  In the default build a stub implementation is used so the
// project can compile without the TensorRT SDK installed.

#include "gpu_eval.h"
#include <future>
#include <vector>
#include <deque>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <chrono>

#ifdef NIKOLA_USE_TENSORRT
#include <NvInfer.h>
#include <cuda_runtime.h>
#include <fstream>
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

namespace {

// Runtime objects for TensorRT execution.  We keep a single engine and one
// execution context per CUDA stream.  A small dispatcher thread collects
// feature vectors into batches and enqueues them on the first stream.  The
// implementation is intentionally lightweight so that the rest of the engine
// can link without the TensorRT SDK being present at build time.
nvinfer1::IRuntime* gRuntime = nullptr;
nvinfer1::ICudaEngine* gEngine = nullptr;
nvinfer1::IExecutionContext* gContext = nullptr;
cudaStream_t gStream{};
int gInputBinding = 0;
int gOutputBinding = 1;
size_t gInputSize = 0;   // number of floats per item
size_t gOutputSize = 0;  // number of floats per item
size_t gMaxBatch = 1;

struct Task {
    std::vector<float> features;
    std::promise<float> promise;
};

std::deque<Task> gQueue;
std::mutex gMutex;
std::condition_variable gCv;
bool gStop = false;
std::thread gDispatch;
std::chrono::milliseconds gFlushMs{1};
size_t gInFlight = 0;

// Helper to read an entire file into memory.
std::vector<char> readFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    return std::vector<char>((std::istreambuf_iterator<char>(f)),
                             std::istreambuf_iterator<char>());
}

void dispatchLoop() {
    std::vector<Task> batch;
    std::vector<float> hostInput;
    std::vector<float> hostOutput;
    while (true) {
        batch.clear();
        {
            std::unique_lock<std::mutex> lock(gMutex);
            gCv.wait_for(lock, gFlushMs, [] {
                return gStop || gQueue.size() >= gMaxBatch;
            });
            if (gStop && gQueue.empty()) break;
            size_t n = std::min(gMaxBatch, gQueue.size());
            gInFlight += n;
            for (size_t i = 0; i < n; ++i) {
                batch.push_back(std::move(gQueue.front()));
                gQueue.pop_front();
            }
        }

        if (batch.empty()) continue;

        hostInput.resize(batch.size() * gInputSize);
        hostOutput.resize(batch.size() * gOutputSize);
        for (size_t i = 0; i < batch.size(); ++i) {
            std::memcpy(hostInput.data() + i * gInputSize,
                        batch[i].features.data(),
                        gInputSize * sizeof(float));
        }
        void* bindings[2] = { nullptr, nullptr };
        cudaMalloc(&bindings[gInputBinding], hostInput.size() * sizeof(float));
        cudaMalloc(&bindings[gOutputBinding], hostOutput.size() * sizeof(float));
        cudaMemcpyAsync(bindings[gInputBinding], hostInput.data(),
                        hostInput.size() * sizeof(float),
                        cudaMemcpyHostToDevice, gStream);
        gContext->setBindingDimensions(gInputBinding,
            nvinfer1::Dims4{static_cast<int>(batch.size()), 1, 1,
                            static_cast<int>(gInputSize)});
        gContext->enqueueV2(bindings, gStream, nullptr);
        cudaMemcpyAsync(hostOutput.data(), bindings[gOutputBinding],
                        hostOutput.size() * sizeof(float),
                        cudaMemcpyDeviceToHost, gStream);
        cudaStreamSynchronize(gStream);
        cudaFree(bindings[gInputBinding]);
        cudaFree(bindings[gOutputBinding]);

        for (size_t i = 0; i < batch.size(); ++i) {
            float val = hostOutput.empty() ? 0.0f : hostOutput[i];
            batch[i].promise.set_value(val);
        }
        {
            std::lock_guard<std::mutex> lock(gMutex);
            gInFlight -= batch.size();
            if (gQueue.empty() && gInFlight == 0) gCv.notify_all();
        }
    }
}

void shutdown() {
    {
        std::lock_guard<std::mutex> lock(gMutex);
        gStop = true;
    }
    gCv.notify_all();
    if (gDispatch.joinable()) gDispatch.join();
    if (gContext) gContext->destroy();
    if (gEngine) gEngine->destroy();
    if (gRuntime) gRuntime->destroy();
    gContext = nullptr;
    gEngine = nullptr;
    gRuntime = nullptr;
    if (gStream) cudaStreamDestroy(gStream);
}

struct ShutdownGuard { ~ShutdownGuard() { shutdown(); } } gShutdown;

} // namespace

void GpuEval::init(const std::string& model_path,
                   const std::string& /*precision*/,
                   int /*device_id*/,
                   size_t max_batch,
                   int /*streams*/) {
    shutdown();
    gRuntime = nvinfer1::createInferRuntime(gLogger);
    std::vector<char> data = readFile(model_path);
    if (!data.empty()) {
        gEngine = gRuntime->deserializeCudaEngine(data.data(), data.size());
        gContext = gEngine ? gEngine->createExecutionContext() : nullptr;
        if (gEngine) {
            gInputBinding = gEngine->getBindingIndex("input");
            gOutputBinding = gEngine->getBindingIndex("output");
            auto inDims = gEngine->getBindingDimensions(gInputBinding);
            auto outDims = gEngine->getBindingDimensions(gOutputBinding);
            gInputSize = 1;
            for (int i = 0; i < inDims.nbDims; ++i) gInputSize *= inDims.d[i];
            gOutputSize = 1;
            for (int i = 0; i < outDims.nbDims; ++i) gOutputSize *= outDims.d[i];
        }
    }
    cudaStreamCreate(&gStream);
    gMaxBatch = std::max<size_t>(1, max_batch);
    gStop = false;
    gDispatch = std::thread(dispatchLoop);
}

std::future<float> GpuEval::submit(const float* features, size_t len) {
    Task t;
    t.features.assign(features, features + len);
    std::future<float> fut = t.promise.get_future();
    {
        std::lock_guard<std::mutex> lock(gMutex);
        gQueue.push_back(std::move(t));
    }
    gCv.notify_all();
    return fut;
}

void GpuEval::flush() {
    std::unique_lock<std::mutex> lock(gMutex);
    gCv.wait(lock, [] { return gQueue.empty() && gInFlight == 0; });
}

} // namespace nikola

#else // !NIKOLA_USE_TENSORRT

// ---------------------------------------------------------------------------
// Fallback CPU implementation used when TensorRT support is disabled.  This
// path retains the same asynchronous API but performs NNUE evaluation on the
// CPU.  To simulate a production GPU backend we batch requests with a short
// timeout so that callers issuing many concurrent evaluations benefit from
// vectorised processing.
// ---------------------------------------------------------------------------

#include "nnue.h"
#include "bitboard.h"

namespace nikola {
namespace {

struct Task {
    std::vector<float> features;
    std::promise<float> promise;
};

std::deque<Task> queue;
std::mutex mtx;
std::condition_variable cv;
bool stopFlag = false;
std::thread dispatcher;
NNUE gNet; // default network instance
size_t g_maxBatch = 1;
size_t inFlight = 0;
std::chrono::milliseconds flushMs{2};

void dispatchLoop() {
    std::vector<Task> batch;
    while (true) {
        batch.clear();
        {
            std::unique_lock<std::mutex> lock(mtx);
            cv.wait_for(lock, flushMs, [] {
                return stopFlag || queue.size() >= g_maxBatch;
            });
            if (stopFlag && queue.empty()) return;
            size_t n = std::min(g_maxBatch, queue.size());
            inFlight += n;
            for (size_t i = 0; i < n; ++i) {
                batch.push_back(std::move(queue.front()));
                queue.pop_front();
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
            bool white = t.features.size() > 12 * 64 && t.features[12 * 64] > 0.0f;
            float score = static_cast<float>(gNet.evaluate(bb, white));
            t.promise.set_value(score);
        }
        {
            std::lock_guard<std::mutex> lock(mtx);
            inFlight -= batch.size();
            if (queue.empty() && inFlight == 0) cv.notify_all();
        }
    }
}

void shutdown() {
    {
        std::lock_guard<std::mutex> lock(mtx);
        stopFlag = true;
    }
    cv.notify_all();
    if (dispatcher.joinable()) dispatcher.join();
}

struct Shutdown { ~Shutdown() { shutdown(); } } guard;

} // namespace

void GpuEval::init(const std::string& /*model_path*/,
                   const std::string& /*precision*/,
                   int /*device_id*/,
                   size_t max_batch,
                   int streams) {
    shutdown();
    {
        std::lock_guard<std::mutex> lock(mtx);
        stopFlag = false;
    }
    g_maxBatch = std::max<size_t>(1, max_batch);
    flushMs = std::chrono::milliseconds(std::max(1, 2 / std::max(1, streams)));
    dispatcher = std::thread(dispatchLoop);
}

std::future<float> GpuEval::submit(const float* features, size_t len) {
    Task t;
    t.features.assign(features, features + len);
    std::future<float> fut = t.promise.get_future();
    {
        std::lock_guard<std::mutex> lock(mtx);
        queue.push_back(std::move(t));
    }
    cv.notify_all();
    return fut;
}

void GpuEval::flush() {
    std::unique_lock<std::mutex> lock(mtx);
    cv.wait(lock, [] { return queue.empty() && inFlight == 0; });
}

} // namespace nikola

#endif // NIKOLA_USE_TENSORRT

