#pragma once

// FlowCudaOutputImage — CUDA→Flutter texture sink with a user-set channel label.

#if defined(WIRE_WITH_CUDA) || (defined(FLOW_FFI_HAS_CUDA) && FLOW_FFI_HAS_CUDA)

#include <flow/core/Env.hpp>
#include <flow/core/IndexableName.hpp>
#include <flow/core/Node.hpp>
#include <flow/core/NodeData.hpp>
#include <flow/core/TypeName.hpp>
#include <flow/core/UUID.hpp>

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

#include <flow_ffi_cuda.h>
#include <wire/CudaDeviceImage.hpp>

FLOW_NAMESPACE_BEGIN

class FlowCudaOutputImage;
template<>
constexpr std::string_view TypeName_v<FlowCudaOutputImage> = "FlowCudaOutputImage";

class FlowCudaOutputImage : public Node
{
  public:
    explicit FlowCudaOutputImage(const UUID& uuid, const std::string& name, std::shared_ptr<Env> env)
        : Node(uuid, TypeName_v<FlowCudaOutputImage>, name, std::move(env))
    {
        AddInput<CudaDeviceImage>("in", "");
        AddInput<std::string>("channel", "");
        AddOutput<int64_t>("texture_id", "Flutter texture ID; re-emitted on dimension change.");
        AddOutput<int64_t>("texture_width", "Width in pixels of the registered texture.");
        AddOutput<int64_t>("texture_height", "Height in pixels of the registered texture.");
    }

    ~FlowCudaOutputImage() override
    {
        std::fprintf(stderr, "[FlowCudaOutputImage] dtor this=%p _texture=%p _w=%d _h=%d\n", static_cast<void*>(this),
                     static_cast<void*>(_texture), _w, _h);
        std::fflush(stderr);
        cleanup_texture();
    }

    void Start() override
    {
        std::fprintf(stderr, "[FlowCudaOutputImage] Start() this=%p\n", static_cast<void*>(this));
        std::fflush(stderr);
    }

    void Stop() override
    {
        std::fprintf(stderr, "[FlowCudaOutputImage] Stop() this=%p _texture=%p\n", static_cast<void*>(this),
                     static_cast<void*>(_texture));
        std::fflush(stderr);
        cleanup_texture();
    }

  protected:
    void Compute() override
    {
        // Diagnostic: log every Nth Compute call so we know the cascade is alive.
        const uint64_t n = _compute_count.fetch_add(1, std::memory_order_relaxed) + 1;
        if (n == 1 || (n % 100) == 0)
        {
            std::fprintf(stderr, "[FlowCudaOutputImage] Compute#%lu this=%p _texture=%p\n",
                         static_cast<unsigned long>(n), static_cast<void*>(this), static_cast<void*>(_texture));
            std::fflush(stderr);
        }

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

        int current_device = -1;
        cudaGetDevice(&current_device);
        if (img.buffer && img.buffer->device() != current_device)
        {
            throw std::runtime_error("FlowCudaOutputImage: producer device (" + std::to_string(img.buffer->device()) +
                                     ") differs from sink device (" + std::to_string(current_device) +
                                     "). v1 only supports single-GPU; "
                                     "use cudaMemcpyPeerAsync or pin both producer and sink to the same device.");
        }

        if (!_texture || img.width != _w || img.height != _h)
        {
            if (_texture)
            {
                flow_ffi_unregister_cuda_flutter_texture(_texture);
                _texture = nullptr;
            }

            int64_t tex_id = 0;
            FlowError err  = flow_ffi_register_cuda_flutter_texture(img.width, img.height, &_texture, &tex_id);

            if (err != FLOW_SUCCESS)
            {
                _texture = nullptr;
                throw std::runtime_error("FlowCudaOutputImage: "
                                         "flow_ffi_register_cuda_flutter_texture failed (code " +
                                         std::to_string(static_cast<int>(err)) + ")");
            }

            _flutter_texture_id = tex_id;
            _w                  = img.width;
            _h                  = img.height;

            SetOutputData(IndexableName{"texture_id"}, std::make_shared<NodeData<int64_t>>(_flutter_texture_id));
            SetOutputData(IndexableName{"texture_width"},
                          std::make_shared<NodeData<int64_t>>(static_cast<int64_t>(_w)));
            SetOutputData(IndexableName{"texture_height"},
                          std::make_shared<NodeData<int64_t>>(static_cast<int64_t>(_h)));
        }

        FlowError err = flow_ffi_cuda_write_into(_texture, img.buffer->ptr(), img.pitch_bytes, img.width, img.height,
                                                 static_cast<void*>(img.ready_event));

        if (err != FLOW_SUCCESS)
        {
            throw std::runtime_error("FlowCudaOutputImage: "
                                     "flow_ffi_cuda_write_into failed (code " +
                                     std::to_string(static_cast<int>(err)) + ")");
        }

        flow_ffi_signal_frame_available(_flutter_texture_id);
    }

  private:
    void cleanup_texture() noexcept
    {
        if (_texture)
        {
            flow_ffi_unregister_cuda_flutter_texture(_texture);
            _texture = nullptr;
        }
        _flutter_texture_id = 0;
        _w                  = 0;
        _h                  = 0;
    }

    FlowCudaTextureHandle _texture = nullptr;
    int64_t _flutter_texture_id    = 0;
    int _w                         = 0;
    int _h                         = 0;
    // Diagnostic counter — see Compute().
    std::atomic<uint64_t> _compute_count{0};
};

FLOW_NAMESPACE_END

#endif // WIRE_WITH_CUDA || FLOW_FFI_HAS_CUDA
