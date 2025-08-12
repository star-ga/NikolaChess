// GPU evaluation service stub for NikolaChess.
//
// This header defines a stub interface for GPU-accelerated evaluation of
// chess positions.  The implementation provided here is a placeholder
// that can be expanded to support batched inference using CUDA,
// TensorRT or other backends.  The API follows the outline
// described in the roadmap: initialize the service with a model and
// batch size, submit feature vectors for evaluation and optionally
// flush any outstanding batches.

#pragma once

#include <future>
#include <cstddef>
#include <string>

namespace nikola {

class GpuEval {
public:
    // Initialise the GPU evaluation service.  The parameters specify
    // the path to a model file, the precision to use (e.g. "fp16",
    // "tf32", "fp8"), the device identifier, the maximum batch
    // size and the number of streams to employ for overlapping data
    // transfer and computation.  The stub implementation ignores all
    // parameters.
    static void init(const std::string& model_path,
                     const std::string& precision,
                     int device_id,
                     size_t max_batch,
                     int streams = 3);

    // Submit a feature vector for evaluation.  The features pointer
    // should reference a contiguous array of length `len`.  A
    // std::future<float> is returned which will hold the evaluation
    // result when ready.  The stub returns an immediate future with
    // a constant value of 0.0f.
    static std::future<float> submit(const float* features, size_t len);

    // Flush any pending batches to the GPU.  In the stub
    // implementation this is a no-op.
    static void flush();

private:
    GpuEval() = delete;
};

} // namespace nikola