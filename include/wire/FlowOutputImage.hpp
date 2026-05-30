#pragma once

// FlowOutputImage — image sink that mirrors FlowFlutterTextureSink's direct-GPU
// path so the Dart texture panel can render via Texture(textureId:) without an
// intermediate ui.Image / Skia upload.
//
// Lazily registers a Flutter GL texture via the FlowGpuTextureOps callback
// table, copies host bytes into the texture per frame via the cascade worker,
// and emits the Flutter texture_id on an output port named "texture_id"
// whenever the dimensions change.
//
// Differences from FlowFlutterTextureSink:
//   - Carries an extra "channel" std::string input used as a user-facing label
//     (no behavioural effect).
//   - Unconditionally registered (no CUDA / GPU-sink capability gate).
//
// Format gate: only flow::Image::Kind::RawRGBA is accepted.  Encoded payloads
// throw — decoding belongs upstream.

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

#include <flow_ffi.h>
#include <wire/Image.hpp>

FLOW_NAMESPACE_BEGIN

class FlowOutputImage;
template<>
constexpr std::string_view TypeName_v<FlowOutputImage> = "FlowOutputImage";

class FlowOutputImage : public Node
{
  public:
    explicit FlowOutputImage(const UUID& uuid, const std::string& name, std::shared_ptr<Env> env)
        : Node(uuid, TypeName_v<FlowOutputImage>, name, std::move(env))
    {
        AddInput<Image>("in", "");
        AddInput<std::string>("channel", "");
        AddOutput<int64_t>("texture_id", "Flutter texture ID; re-emitted whenever the registered texture is "
                                         "created or recreated due to a dimension change.  Dart widgets bind "
                                         "Texture(textureId: value) to this port.");
    }

    ~FlowOutputImage() override
    {
        std::fprintf(stderr, "[FlowOutputImage] dtor this=%p _texture=%p _w=%d _h=%d\n", static_cast<void*>(this),
                     static_cast<void*>(_texture), _w, _h);
        std::fflush(stderr);
        cleanup_texture();
    }

    void Start() override
    {
        std::fprintf(stderr, "[FlowOutputImage] Start() this=%p\n", static_cast<void*>(this));
        std::fflush(stderr);
    }

    void Stop() override
    {
        std::fprintf(stderr, "[FlowOutputImage] Stop() this=%p _texture=%p\n", static_cast<void*>(this),
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
            std::fprintf(stderr, "[FlowOutputImage] Compute#%lu this=%p _texture=%p\n", static_cast<unsigned long>(n),
                         static_cast<void*>(this), static_cast<void*>(_texture));
            std::fflush(stderr);
        }

        auto input_data = GetInputData<Image>(IndexableName{"in"});
        if (!input_data)
        {
            return;
        }

        const Image& img = input_data->Get();

        if (img.kind != Image::Kind::RawRGBA)
        {
            throw std::runtime_error("FlowOutputImage: received Kind::Encoded image — "
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
                throw std::runtime_error("FlowOutputImage: "
                                         "flow_ffi_register_flutter_texture failed (code " +
                                         std::to_string(static_cast<int>(err)) + ")");
            }

            _flutter_texture_id = tex_id;
            _w                  = img.width;
            _h                  = img.height;

            SetOutputData(IndexableName{"texture_id"}, std::make_shared<NodeData<int64_t>>(_flutter_texture_id));
        }

        FlowError err = flow_ffi_upload_to_texture(_texture, img.bytes->data(), img.row_stride, img.width, img.height);

        if (err != FLOW_SUCCESS)
        {
            throw std::runtime_error("FlowOutputImage: "
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
    // Diagnostic counter — see Compute().
    std::atomic<uint64_t> _compute_count{0};
};

FLOW_NAMESPACE_END
