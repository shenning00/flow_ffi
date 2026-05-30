#pragma once

// CudaToImage — CUDA→CPU adapter node for the flow-wire interworking layer.
// Converts a GPU-resident flow::CudaDeviceImage into a host-side flow::Image
// (kind=RawRGBA, format=RGBA8888) by performing a synchronous device→host copy
// via cudaMemcpy2D.
//
// Design decisions:
//
//   Synchronization: We call cudaEventSynchronize(img.ready_event) rather than
//   cudaStreamWaitEvent on a dedicated stream.  The cudaEventSynchronize path
//   blocks the calling thread until the producer's ready_event fires, which is
//   simpler and avoids the need to manage a per-node auxiliary stream.  Because
//   this is explicitly the fallback/interworking path (Option A in the spec),
//   the synchronous stall is acceptable — callers that need async throughput
//   should be on the Option B Flutter-texture path instead.
//
//   ready_event == nullptr: If the producer did not record a ready_event (valid
//   per flow_ffi_cuda.h's convention), we skip the cudaEventSynchronize call
//   and assume the buffer is already settled (e.g. the producer called
//   cudaDeviceSynchronize before handing off the CudaDeviceImage).
//
//   CUDA errors: Any non-cudaSuccess return from cudaMemcpy2D is surfaced via
//   std::runtime_error, which flow-core's Node::InvokeCompute catch-all
//   converts into an error callback.  This matches the convention used by
//   ImageOpenNode (throws on unreadable file).
//
//   Pitch unpacking: CudaDeviceImage::pitch_bytes is the row stride on the
//   device side (may be wider than width*4 due to device alignment).  We read
//   only width*4 bytes per row by passing width*4 as dpitch to cudaMemcpy2D,
//   producing a tightly-packed RGBA8888 host buffer with row_stride = width*4.
//
// Only compiled when WIRE_WITH_CUDA or FLOW_FFI_HAS_CUDA is defined,
// mirroring CudaDeviceImage.hpp's guard.

#if defined(WIRE_WITH_CUDA) || (defined(FLOW_FFI_HAS_CUDA) && FLOW_FFI_HAS_CUDA)

#include <flow/core/Env.hpp>
#include <flow/core/IndexableName.hpp>
#include <flow/core/Node.hpp>
#include <flow/core/NodeData.hpp>
#include <flow/core/TypeName.hpp>
#include <flow/core/UUID.hpp>

#include <atomic>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <cuda_runtime.h>
#include <wire/CudaDeviceImage.hpp>
#include <wire/Image.hpp>

FLOW_NAMESPACE_BEGIN

class CudaToImage;
template<>
constexpr std::string_view TypeName_v<CudaToImage> = "CudaToImage";

class CudaToImage : public Node
{
  public:
    explicit CudaToImage(const UUID& uuid, const std::string& name, std::shared_ptr<Env> env)
        : Node(uuid, TypeName_v<CudaToImage>, name, std::move(env))
    {
        AddInput<CudaDeviceImage>("in", "GPU-resident RGBA8 image");
        AddOutput<Image>("out", "Host-side RawRGBA flow::Image (RGBA8888, tightly packed)");
    }

    ~CudaToImage() override = default;

  protected:
    void Compute() override
    {
        auto input_data = GetInputData<CudaDeviceImage>(IndexableName{"in"});
        if (!input_data)
        {
            return;
        }

        const CudaDeviceImage& img = input_data->Get();

        if (!img.buffer || !img.buffer->ptr())
        {
            return;
        }
        if (img.width <= 0 || img.height <= 0)
        {
            return;
        }

        // Synchronize against the producer's ready_event before reading the
        // device buffer.  Null ready_event means the producer already ensured
        // visibility (e.g. via cudaDeviceSynchronize), so skip the call.
        if (img.ready_event != nullptr)
        {
            const cudaError_t sync_err = cudaEventSynchronize(img.ready_event);
            if (sync_err != cudaSuccess)
            {
                throw std::runtime_error(std::string("CudaToImage: cudaEventSynchronize failed: ") +
                                         cudaGetErrorString(sync_err));
            }
        }

        const int32_t width           = img.width;
        const int32_t height          = img.height;
        const int32_t host_row_stride = width * 4; // tightly packed RGBA8

        auto bytes = std::make_shared<std::vector<uint8_t>>(static_cast<std::size_t>(host_row_stride) *
                                                            static_cast<std::size_t>(height));

        // cudaMemcpy2D(dst, dpitch, src, spitch, width_bytes, height, kind)
        //   dst        — host buffer
        //   dpitch     — host row stride (tightly packed = width * 4)
        //   src        — device buffer pointer
        //   spitch     — device row stride (may be padded)
        //   width_bytes — bytes to copy per row (width * 4, not the full pitch)
        //   height     — number of rows
        const cudaError_t copy_err = cudaMemcpy2D(bytes->data(), static_cast<std::size_t>(host_row_stride),
                                                  img.buffer->ptr(), static_cast<std::size_t>(img.pitch_bytes),
                                                  static_cast<std::size_t>(host_row_stride), // width_bytes
                                                  static_cast<std::size_t>(height), cudaMemcpyDeviceToHost);

        if (copy_err != cudaSuccess)
        {
            throw std::runtime_error(std::string("CudaToImage: cudaMemcpy2D failed: ") + cudaGetErrorString(copy_err));
        }

        Image out;
        out.kind            = Image::Kind::RawRGBA;
        out.format          = Image::Format::RGBA8888;
        out.width           = width;
        out.height          = height;
        out.row_stride      = host_row_stride;
        out.bytes           = std::move(bytes);
        out.content_version = next_version();

        SetOutputData(IndexableName{"out"}, std::make_shared<NodeData<Image>>(std::move(out)));
    }

  private:
    static uint64_t next_version() noexcept
    {
        static std::atomic<uint64_t> counter{0};
        return counter.fetch_add(1, std::memory_order_relaxed) + 1;
    }
};

FLOW_NAMESPACE_END

#endif // WIRE_WITH_CUDA || FLOW_FFI_HAS_CUDA
