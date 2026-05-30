#pragma once

// FlowFlutterTextureSink — CPU-host→Flutter texture sink node (P12).
//
// Receives a flow::Image (RGBA8 raw) on its "in" input, lazily registers a
// Flutter GL texture via the P12 FlowGpuTextureOps callback table, and
// per-frame copies the host bytes into the texture via a synchronous memcpy
// inside submit_frame (called from the cascade worker; no GL context needed).
//
// Emits the Flutter texture_id on an output port named "texture_id" whenever
// the dimensions change (or a new texture is registered).  Mirrors
// FlowFlutterCudaPreview's architecture identically with host bytes instead of
// CUDA device pointers.
//
// Design decisions:
//   Format gate: only flow::Image::Kind::RawRGBA is accepted.  Kind::Encoded
//   images need decoding first — that is an upstream concern (ImageOpenNode
//   already produces RawRGBA; PreviewNode decodes in Dart).  Encoded payloads
//   are rejected with a clear error message.
//
//   Lazy texture creation: dimensions are unknown until the first Compute call.
//   Start() is intentionally a no-op.
//
//   Dimension change: old texture is unregistered and a new one allocated.
//   texture_id output is re-emitted so the Dart Texture widget can rebind.
//
//   Stop() safety: guards on _texture != nullptr before calling
//   flow_ffi_unregister_flutter_texture (node may reach Stop without input).
//
//   Unconditional compilation: no CUDA gate.  Available on all platforms that
//   have a GPU texture ops vtable injected by the Flutter plugin.
//
//   Error surfacing: FFI failures throw std::runtime_error, which
//   InvokeCompute catches and forwards to OnError.Broadcast — same convention
//   as FlowFlutterCudaPreview.

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

#include <flow_ffi.h>
#include <wire/Image.hpp>

FLOW_NAMESPACE_BEGIN

class FlowFlutterTextureSink;
template<>
constexpr std::string_view TypeName_v<FlowFlutterTextureSink> = "FlowFlutterTextureSink";

class FlowFlutterTextureSink : public Node
{
  public:
    explicit FlowFlutterTextureSink(const UUID& uuid, const std::string& name, std::shared_ptr<Env> env)
        : Node(uuid, TypeName_v<FlowFlutterTextureSink>, name, std::move(env))
    {
        AddInput<Image>("in", "");
        AddOutput<int64_t>("texture_id", "Flutter texture ID; re-emitted whenever the registered texture is "
                                         "created or recreated due to a dimension change.  Dart widgets bind "
                                         "Texture(textureId: value) to this port.");
    }

    ~FlowFlutterTextureSink() override { cleanup_texture(); }

    void Start() override
    {
        // Lazy — dims unknown until first Compute.
    }

    void Stop() override { cleanup_texture(); }

  protected:
    void Compute() override
    {
        auto input_data = GetInputData<Image>(IndexableName{"in"});
        if (!input_data)
        {
            return;
        }

        const Image& img = input_data->Get();

        // Reject encoded images — decode upstream first.
        if (img.kind != Image::Kind::RawRGBA)
        {
            throw std::runtime_error("FlowFlutterTextureSink: received Kind::Encoded image — "
                                     "decode to RawRGBA upstream (ImageOpenNode produces RawRGBA)");
        }

        if (!img.bytes || img.bytes->empty())
        {
            return;
        }
        if (img.width <= 0 || img.height <= 0 || img.row_stride <= 0)
        {
            return;
        }

        // Lazy-create or recreate the Flutter texture on dimension change.
        if (!_texture || img.width != _w || img.height != _h)
        {
            if (_texture)
            {
                flow_ffi_unregister_flutter_texture(_texture);
                _texture = nullptr;
            }

            int64_t tex_id = 0;
            FlowError err  = flow_ffi_register_flutter_texture(img.width, img.height, &_texture, &tex_id);

            if (err != FLOW_SUCCESS)
            {
                _texture = nullptr;
                throw std::runtime_error("FlowFlutterTextureSink: "
                                         "flow_ffi_register_flutter_texture failed (code " +
                                         std::to_string(static_cast<int>(err)) + ")");
            }

            _flutter_texture_id = tex_id;
            _w                  = img.width;
            _h                  = img.height;

            SetOutputData(IndexableName{"texture_id"}, std::make_shared<NodeData<int64_t>>(_flutter_texture_id));
        }

        // Per-frame host→texture copy (synchronous memcpy inside submit_frame).
        FlowError err = flow_ffi_upload_to_texture(_texture, img.bytes->data(), img.row_stride, img.width, img.height);

        if (err != FLOW_SUCCESS)
        {
            throw std::runtime_error("FlowFlutterTextureSink: "
                                     "flow_ffi_upload_to_texture failed (code " +
                                     std::to_string(static_cast<int>(err)) + ")");
        }
    }

  private:
    void cleanup_texture() noexcept
    {
        if (_texture)
        {
            flow_ffi_unregister_flutter_texture(_texture);
            _texture = nullptr;
        }
        _flutter_texture_id = 0;
        _w                  = 0;
        _h                  = 0;
    }

    FlowGpuTextureHandle _texture = nullptr;
    int64_t _flutter_texture_id   = 0;
    int _w                        = 0;
    int _h                        = 0;
};

FLOW_NAMESPACE_END
