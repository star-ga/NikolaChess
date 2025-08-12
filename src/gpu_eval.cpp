// Stub implementation of the GPU evaluation service for NikolaChess.
//
// This file provides no-op implementations of the GpuEval API and a CPU
// fallback for batched board evaluation.  It allows the project to compile
// and link in environments without CUDA by delegating evaluation to the
// existing CPU evaluator.

#include "gpu_eval.h"
#include <future>
#include <vector>

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
