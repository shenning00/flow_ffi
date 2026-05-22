#pragma once

// flow_image.hpp — the C++ value type that travels through flow-core node
// ports as image data.  Wire-equivalent of the FlowImageDescriptor in
// flow_ffi.h.  See FLOW_RUN.html Appendix B §B.4.3 for the design rationale.
//
// Lifetime: the byte buffer is held by a shared_ptr<vector<uint8_t>>, so
// fanning out to multiple downstream nodes does not copy the pixels; only
// the shared_ptr refcount is bumped.  flow::Image itself is a cheap-to-copy
// value type (a few scalars plus a shared_ptr).
//
// Threading: producers construct flow::Image on worker threads; consumers
// (PreviewNode etc.) only inspect it.  All decode work is deferred to the
// Dart root isolate (see P11) — there is no GL/Metal/Vulkan call here.

#include <cstdint>
#include <memory>
#include <vector>

namespace flow {

struct Image {
    enum class Kind { Encoded, RawRGBA };
    enum class Format { RGBA8888 };

    Kind kind = Kind::Encoded;
    Format format = Format::RGBA8888; // ignored for Encoded
    int32_t width = -1;
    int32_t height = -1;
    int32_t row_stride = 0; // bytes per row (RawRGBA only)
    std::shared_ptr<std::vector<uint8_t>> bytes; // refcounted payload
    uint64_t content_version = 0; // monotonic per producing port

    [[nodiscard]] std::size_t size_bytes() const noexcept {
        return bytes ? bytes->size() : 0;
    }
};

} // namespace flow
