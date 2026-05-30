#pragma once

// FlowFlutterCudaPreview — CUDA→Flutter texture sink node (Phase E).
//
// Receives a GPU-resident flow::CudaDeviceImage on its "in" input, lazily
// registers a Flutter GL texture via the Phase D FlowTextureOps callback
// table, and per-frame copies the device buffer into the texture.  Emits the
// Flutter texture_id on an output port named "texture_id" whenever the
// dimensions change (or a new texture is registered).
//
// Design decisions:
//
//   Lazy texture creation: dimensions are not known until the first Compute
//   call, so Start() is intentionally empty.  The texture is registered on
//   the first call to Compute() that receives a valid CudaDeviceImage.
//
//   Dimension change handling: if the input image's width or height changes,
//   the old Flutter texture is unregistered and a new one is allocated.  The
//   texture_id output is re-emitted so that the Dart Texture widget can rebind.
//
//   texture_id output port: A real AddOutput<int64_t>("texture_id","") port is
//   declared in the constructor so that SetOutputData (and the node's
//   OnSetOutput event) work without modification.  This allows Dart-side
//   FlowBridge listeners to observe the texture_id via flow_node_on_set_output
//   exactly as they would any other output.  The synthetic-broadcast-only path
//   was considered but rejected because SetOutputData calls _output_ports.at()
//   which throws std::out_of_range for undeclared ports, and calling
//   OnSetOutput.Broadcast directly bypasses port storage.  A real port is the
//   cleaner solution.
//
//   Stop() safety: called both when the graph removes the node and on engine
//   shutdown.  The node may reach Stop() having never received input (e.g.
//   added to a graph but never wired), so all cleanup paths guard on
//   _texture != nullptr before calling flow_ffi_unregister_cuda_flutter_texture.
//
//   Error surfacing: FFI failures in register/write throw std::runtime_error,
//   which InvokeCompute catches and forwards to OnError.Broadcast.  This
//   mirrors CudaToImage's convention and keeps Compute() exception-safe.
//
// Only compiled when WIRE_WITH_CUDA or FLOW_FFI_HAS_CUDA is defined,
// mirroring CudaDeviceImage.hpp's guard exactly.

#if defined(WIRE_WITH_CUDA) || (defined(FLOW_FFI_HAS_CUDA) && FLOW_FFI_HAS_CUDA)

#include <flow/core/Env.hpp>
#include <flow/core/IndexableName.hpp>
#include <flow/core/Node.hpp>
#include <flow/core/NodeData.hpp>
#include <flow/core/TypeName.hpp>
#include <flow/core/UUID.hpp>

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

#include <flow_ffi_cuda.h>
#include <wire/CudaDeviceImage.hpp>

FLOW_NAMESPACE_BEGIN

class FlowFlutterCudaPreview;
template<>
constexpr std::string_view TypeName_v<FlowFlutterCudaPreview> = "FlowFlutterCudaPreview";

class FlowFlutterCudaPreview : public Node
{
  public:
    explicit FlowFlutterCudaPreview(const UUID& uuid, const std::string& name, std::shared_ptr<Env> env)
        : Node(uuid, TypeName_v<FlowFlutterCudaPreview>, name, std::move(env))
    {
        AddInput<CudaDeviceImage>("in", "");
        AddOutput<int64_t>("texture_id", "Flutter texture ID; re-emitted whenever the registered texture is "
                                         "created or recreated due to a dimension change.  Dart widgets bind "
                                         "Texture(textureId: value) to this port.");
    }

    ~FlowFlutterCudaPreview() override
    {
        // Best-effort cleanup: Stop() may not have been called if the node was
        // destroyed without going through a graph remove path.
        cleanup_texture();
    }

    void Start() override
    {
        // Lazy creation — dims are unknown until the first Compute call.
        // Nothing to do here.
    }

    void Stop() override { cleanup_texture(); }

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

        // CR2 (spec §10 risk register): validate that the producer's device
        // matches the sink's active device before attempting the device→texture
        // copy.  On a multi-GPU host the producer node may have allocated its
        // CudaBuffer on device N while the Flutter texture was registered (or
        // is about to be registered) on device M; a silent cudaMemcpy across
        // the device boundary corrupts the texture with no CUDA error code.
        //
        // We query the *calling thread's* active device via cudaGetDevice —
        // the same device the texture registration and write calls below will
        // run on.  The producer device is authoritative from CudaBuffer::device()
        // (set at allocation time and immutable thereafter).
        //
        // v1 policy: raise a per-node error (surfaces via OnError.Broadcast →
        // Dart P5 error badge) so the user can see exactly which device pair
        // caused the mismatch.
        //
        // v2 follow-up: optional NVLink peer-copy bridge via
        // cudaMemcpyPeerAsync; out of scope for P13 v1.
        int current_device = -1;
        cudaGetDevice(&current_device);
        if (img.buffer && img.buffer->device() != current_device)
        {
            throw std::runtime_error("FlowFlutterCudaPreview: producer device (" +
                                     std::to_string(img.buffer->device()) + ") differs from sink device (" +
                                     std::to_string(current_device) +
                                     "). v1 only supports single-GPU; "
                                     "use cudaMemcpyPeerAsync or pin both producer and sink to the same device.");
        }

        // Lazy-create or recreate the Flutter texture when dimensions change.
        if (!_texture || img.width != _w || img.height != _h)
        {
            // Release the old texture before allocating a new one.
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
                throw std::runtime_error("FlowFlutterCudaPreview: "
                                         "flow_ffi_register_cuda_flutter_texture failed (code " +
                                         std::to_string(static_cast<int>(err)) + ")");
            }

            _flutter_texture_id = tex_id;
            _w                  = img.width;
            _h                  = img.height;

            // Emit the new texture_id so Dart can rebind Texture(textureId: ...).
            // Only emitted on dimension change, not every frame.
            SetOutputData(IndexableName{"texture_id"}, std::make_shared<NodeData<int64_t>>(_flutter_texture_id));
        }

        // Per-frame device→texture copy.
        FlowError err = flow_ffi_cuda_write_into(_texture, img.buffer->ptr(), img.pitch_bytes, img.width, img.height,
                                                 static_cast<void*>(img.ready_event));

        if (err != FLOW_SUCCESS)
        {
            throw std::runtime_error("FlowFlutterCudaPreview: "
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
};

FLOW_NAMESPACE_END

#endif // WIRE_WITH_CUDA || FLOW_FFI_HAS_CUDA
