/**
 * Copyright (c) 2017 Darius Rückert
 * Licensed under the MIT License.
 * See LICENSE file for more information.
 */

#pragma once

#include "saiga/cuda/cuda.h"

namespace Saiga
{
namespace CUDA
{
/**
 * A simple c++ wrapper for cuda events
 *
 * Usage Example:
 *
 * CudaEvent start, stop;
 * start.record();
 * // ...
 * stop.record();
 * stop.synchronize();
 * float time = CudaEvent::elapsedTime(start,stop);
 *
 */
class SAIGA_CUDA_API CudaEvent
{
   public:
    cudaEvent_t event = 0;


    CudaEvent() { create(); }
    ~CudaEvent() { destroy(); }

    void destroy()
    {
        if (event)
        {
            CHECK_CUDA_ERROR(cudaEventDestroy(event));
            event = 0;
        }
    }

    void create()
    {
        if (!event)
        {
            CHECK_CUDA_ERROR(cudaEventCreate(&event));
        }
    }

    void reset()
    {
        destroy();
        create();
    }

    // Place this event into the command stream
    void record(cudaStream_t stream = 0) { CHECK_CUDA_ERROR(cudaEventRecord(event, stream)); }

    // Non-block stream wait
    void wait(cudaStream_t stream) { CHECK_CUDA_ERROR(cudaStreamWaitEvent(stream, event, 0)); }

    // Wait until this event is completed
    void synchronize() { CHECK_CUDA_ERROR(cudaEventSynchronize(event)); }

    // Test if the event is completed (returns immediately)
    bool isCompleted() { return cudaEventQuery(event) == cudaSuccess; }



    static float elapsedTime(CudaEvent& first, CudaEvent& second)
    {
        float time;
        CHECK_CUDA_ERROR(cudaEventElapsedTime(&time, first, second));
        return time;
    }

    operator cudaEvent_t() const { return event; }
};

}  // namespace CUDA
}  // namespace Saiga
