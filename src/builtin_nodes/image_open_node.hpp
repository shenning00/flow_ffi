#pragma once

// image_open_node.hpp — built-in source node for the image preview pipeline.
//
// Single std::string input "path", single flow::Image output "image".
// Compute() opens the file in binary mode, slurps the bytes, and wraps
// them in a flow::Image (kind=Encoded).  The downstream Dart pipeline
// (P11) decodes via Skia, so we deliberately do no format-aware work
// here — any byte stream Skia can decode (PNG, JPEG, GIF, WebP, etc.)
// will render.
//
// Registered as "Editor.ImageOpen" on flow_env_create; mirrors the
// PreviewNode pattern (see preview_node.hpp).  Combined, the two
// built-ins give the editor a load-and-render path with zero extra
// .flowmod fixtures.

#include <flow/core/Env.hpp>
#include <flow/core/IndexableName.hpp>
#include <flow/core/Node.hpp>
#include <flow/core/TypeName.hpp>
#include <flow/core/UUID.hpp>

#include <atomic>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "flow_image.hpp"

namespace flow::ffi::builtin {

class ImageOpenNode : public flow::Node {
  public:
    explicit ImageOpenNode(const flow::UUID& uuid, const std::string& name,
                           std::shared_ptr<flow::Env> env)
        : flow::Node(uuid, flow::TypeName_v<ImageOpenNode>, name, std::move(env)) {
        AddInput<std::string>("path", "");
        AddOutput<flow::Image>("image", "");
    }

    ~ImageOpenNode() override = default;

  protected:
    void Compute() override {
        auto path_data = GetInputData<std::string>(flow::IndexableName{"path"});
        if (!path_data) {
            // Either nothing connected or the input is still default —
            // skip without surfacing an error.  The downstream Preview
            // widget will show "— no value —" until a path arrives.
            return;
        }
        const std::string& path = path_data->Get();
        if (path.empty()) {
            return;
        }

        std::ifstream file(path, std::ios::binary);
        if (!file) {
            throw std::runtime_error("ImageOpen: failed to open " + path);
        }

        // Slurp the file.  Reserve+assign avoids the iterator-based
        // copy's repeated reallocations; for typical 1080p JPEGs (~500 KB)
        // the difference is in the microseconds, but it matters for
        // multi-MB PNG inputs.
        auto bytes = std::make_shared<std::vector<uint8_t>>();
        file.seekg(0, std::ios::end);
        const auto end_pos = file.tellg();
        if (end_pos > 0) {
            bytes->resize(static_cast<std::size_t>(end_pos));
            file.seekg(0, std::ios::beg);
            file.read(reinterpret_cast<char*>(bytes->data()),
                      static_cast<std::streamsize>(end_pos));
        }
        if (bytes->empty()) {
            throw std::runtime_error("ImageOpen: empty or unreadable file " + path);
        }

        Image img;
        img.kind = Image::Kind::Encoded;
        img.format = Image::Format::RGBA8888; // unused for Encoded
        img.width = -1;                       // resolved by Skia at decode time
        img.height = -1;
        img.row_stride = 0;
        img.bytes = std::move(bytes);
        img.content_version = next_version();

        SetOutputData(flow::IndexableName{"image"},
                      std::make_shared<flow::NodeData<flow::Image>>(std::move(img)));
    }

  private:
    // Per-instance monotonic content_version.  Process-wide uniqueness is
    // already handled by the flow_data_create_image_* counter (see
    // image_bridge.cpp); this counter is for outputs that bypass those
    // factories — i.e. anything constructed in-Compute and handed to
    // SetOutputData.  Strictly monotonic within a single node lifetime,
    // which is all the Dart coalescer requires for dedup.
    static uint64_t next_version() noexcept {
        static std::atomic<uint64_t> counter{0};
        return counter.fetch_add(1, std::memory_order_relaxed) + 1;
    }
};

} // namespace flow::ffi::builtin
