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

namespace nikola {

void GpuEval::init(const std::string& /*model_path*/,
                   const std::string& /*precision*/,
                   int /*device_id*/,
                   size_t /*max_batch*/,
                   int /*streams*/) {
    // No-op in stub.  A real implementation would load the model,
    // allocate GPU resources and prepare streams here.
}

std::future<float> GpuEval::submit(const float* /*features*/, size_t /*len*/) {
    // Immediately return a ready future containing a constant
    // evaluation score.  In a real implementation this would
    // enqueue the features for batched processing on the GPU and
    // return a future representing the asynchronous result.
    std::promise<float> p;
    p.set_value(0.0f);
    return p.get_future();
}

void GpuEval::flush() {
    // Nothing to flush in stub.  A real implementation would
    // trigger any pending batches to be processed.
}

} // namespace nikola

#endif // NIKOLA_USE_TENSORRT

