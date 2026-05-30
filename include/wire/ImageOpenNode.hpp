#pragma once

// ImageOpenNode — source node that reads bytes from a path and emits an
// Encoded flow::Image for downstream consumers (decoders, previewers,
// cv::Mat adapters).  Format-agnostic: any byte stream the consumer can
// decode (PNG, JPEG, GIF, WebP, …) works.
//
// Single std::string input "path", single flow::Image output "image".
// Lives in `wire` rather than `flow_ffi` because it has no FFI or
// frontend coupling — it only produces a wire type.

#include <flow/core/Env.hpp>
#include <flow/core/IndexableName.hpp>
#include <flow/core/Node.hpp>
#include <flow/core/TypeName.hpp>
#include <flow/core/UUID.hpp>

#include <atomic>
#include <cstdint>
#include <fstream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <wire/Image.hpp>

FLOW_NAMESPACE_BEGIN

// Forward declaration + bare TypeName so persisted graphs / UI labels see
// just "ImageOpenNode" instead of the compiler-derived "flow::ImageOpenNode".
// Matches the convention used by flow-nodes (Crop.hpp et al.).
class ImageOpenNode;
template<>
constexpr std::string_view TypeName_v<ImageOpenNode> = "ImageOpenNode";

class ImageOpenNode : public Node
{
  public:
    explicit ImageOpenNode(const UUID& uuid, const std::string& name, std::shared_ptr<Env> env)
        : Node(uuid, TypeName_v<ImageOpenNode>, name, std::move(env))
    {
        AddInput<std::string>("path", "");
        AddOutput<Image>("image", "");
    }

    ~ImageOpenNode() override = default;

  protected:
    void Compute() override
    {
        auto path_data = GetInputData<std::string>(IndexableName{"path"});
        if (!path_data)
        {
            // Either nothing connected or the input is still default — skip
            // without surfacing an error.  Downstream consumers will show a
            // "no value" placeholder until a path arrives.
            return;
        }
        const std::string& path = path_data->Get();
        if (path.empty())
        {
            return;
        }

        std::ifstream file(path, std::ios::binary);
        if (!file)
        {
            throw std::runtime_error("ImageOpen: failed to open " + path);
        }

        // Reserve+read avoids the iterator-copy's repeated reallocations.
        auto bytes = std::make_shared<std::vector<uint8_t>>();
        file.seekg(0, std::ios::end);
        const auto end_pos = file.tellg();
        if (end_pos > 0)
        {
            bytes->resize(static_cast<std::size_t>(end_pos));
            file.seekg(0, std::ios::beg);
            file.read(reinterpret_cast<char*>(bytes->data()), static_cast<std::streamsize>(end_pos));
        }
        if (bytes->empty())
        {
            throw std::runtime_error("ImageOpen: empty or unreadable file " + path);
        }

        Image img;
        img.kind            = Image::Kind::Encoded;
        img.format          = Image::Format::RGBA8888; // unused for Encoded
        img.width           = -1;                      // resolved by the consumer at decode time
        img.height          = -1;
        img.row_stride      = 0;
        img.bytes           = std::move(bytes);
        img.content_version = next_version();

        SetOutputData(IndexableName{"image"}, std::make_shared<NodeData<Image>>(std::move(img)));
    }

  private:
    // Per-process monotonic content_version for outputs constructed inside
    // Compute() (vs. through the flow_ffi image factories, which carry their
    // own counter).  Strictly monotonic within a single node lifetime is all
    // the frontend coalescer needs for dedup.
    static uint64_t next_version() noexcept
    {
        static std::atomic<uint64_t> counter{0};
        return counter.fetch_add(1, std::memory_order_relaxed) + 1;
    }
};

FLOW_NAMESPACE_END
