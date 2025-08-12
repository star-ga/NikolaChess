// Stub implementation of the GPU evaluation service for NikolaChess.
//
// This file provides no-op implementations of the GpuEval API.  It
// allows the project to compile and link while a full GPU-backed
// evaluation service is developed.  The submit() function returns an
// immediately ready future containing a default score of 0.0f.  When
// adding a real implementation, these functions should set up
// buffers, streams and kernel launches as outlined in the roadmap.

#include "gpu_eval.h"

#include <future>

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