#pragma once

// CudaDeviceImage.hpp — GPU-resident image wire type for flow-core node ports.
// Relocated from flow_ffi/src/ to wire/ per post-P10/P11 layering — see
// the layering callout at the top of FLOW_RUN_CUDA_IMAGES.html §4.2.
//
// This header is gated by WIRE_WITH_CUDA.  TUs that include wire
// headers without enabling the CUDA gate will never see cuda_runtime.h.

// This header is active when either the wire CUDA gate (WIRE_WITH_CUDA)
// or the flow_ffi CUDA gate (FLOW_FFI_HAS_CUDA) is set.  The former is defined
// when wire itself is compiled with CUDA; the latter is defined by flow_ffi's
// compile definitions and is visible to any flow_ffi TU that includes this header.
#if defined(WIRE_WITH_CUDA) || (defined(FLOW_FFI_HAS_CUDA) && FLOW_FFI_HAS_CUDA)

#include <flow/core/TypeName.hpp>

#include <memory>

#include <cuda_runtime.h>

FLOW_NAMESPACE_BEGIN

// RAII wrapper around a CUDA device allocation.  Memory is freed on the
// owning stream (cudaFreeAsync) so that in-flight kernel work on that stream
// can complete before the allocation is returned to the pool.
class CudaBuffer
{
  public:
    CudaBuffer(size_t size, int device, cudaStream_t free_stream)
        : _ptr(nullptr), _size(size), _device(device), _free_stream(free_stream)
    {
        cudaSetDevice(device);
        cudaMallocAsync(&_ptr, size, free_stream);
    }

    ~CudaBuffer()
    {
        if (_ptr) cudaFreeAsync(_ptr, _free_stream);
    }

    void* ptr() const { return _ptr; }
    size_t size() const { return _size; }
    int device() const { return _device; }

    CudaBuffer(const CudaBuffer&)            = delete;
    CudaBuffer& operator=(const CudaBuffer&) = delete;

  private:
    void* _ptr;
    size_t _size;
    int _device;
    cudaStream_t _free_stream;
};

// The wire type that flows through node ports for GPU-resident images.
// A cheap-to-copy value type: the pixel data stays on the device; only
// the shared_ptr refcount is bumped when fanning out to multiple consumers.
//
// ready_event is the producer→sink synchronization primitive.  The CUDA
// producer records it at the end of its kernel; the sink does
// cudaStreamWaitEvent on its own stream before mapping the Flutter texture.
// Producer and sink can run on different CUDA streams without races.
struct CudaDeviceImage
{
    std::shared_ptr<CudaBuffer> buffer; // refcounted device allocation
    int32_t width            = 0;
    int32_t height           = 0;
    int32_t pitch_bytes      = 0;       // row stride in bytes
    uint64_t content_version = 0;       // monotonic; used by coalescer to dedup
    cudaEvent_t ready_event  = nullptr; // signal: producer is done writing
};

FLOW_NAMESPACE_END

// TypeName specialization so flow-core's port-type machinery returns the
// stable string "flow::CudaDeviceImage" rather than the compiler's mangled
// __PRETTY_FUNCTION__ form.  Mirrors the pattern used for other node classes
// in wire (e.g. ImageOpenNode.hpp).
template<>
constexpr std::string_view flow::TypeName_v<flow::CudaDeviceImage> = "flow::CudaDeviceImage";

#endif // WIRE_WITH_CUDA || FLOW_FFI_HAS_CUDA
