#include "flow_ffi.h"

#include <flow/core/NodeData.hpp>
#include <flow/core/TypeName.hpp>

#include <atomic>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "error_handling.hpp"
#include "flow_image.hpp"
#include "handle_manager.hpp"
#include "node_data_wrapper.hpp"

// image_bridge.cpp — P10 image primitives.  Mirrors the pattern used by
// type_conversions.cpp for the primitive flow_data_create_* symbols: the
// payload is wrapped in flow::NodeData<flow::Image>, stored in a
// NodeDataWrapper, and surfaced to Dart as an opaque FlowNodeDataHandle.
//
// Lifetime contract (FLOW_RUN.html §B.4.4):
//   - bytes pointer returned by flow_data_image_borrow is a view into the
//     shared_ptr<vector<uint8_t>> owned by the handle.  Valid until the
//     handle's refcount drops to zero (typically via flow_data_destroy).
//   - The shared_ptr lets the producing C++ side, intermediate
//     SharedNodeData copies, and the borrowed view all share ownership
//     of the same vector without copying pixels.

using namespace flow;

namespace {

// Construct a flow::NodeData<flow::Image> from raw fields and wrap it in
// the cross-TU NodeDataWrapper used by the handle registry.
SharedNodeData wrap_image(Image img) {
    return std::make_shared<NodeData<Image>>(std::move(img));
}

// Monotonic content-version generator.  Producers don't expose a way to
// supply their own version yet, so the FFI assigns one on each create-call.
// 64-bit at 1 MHz creation rate still wraps in ~580k years — non-issue.
uint64_t next_content_version() noexcept {
    static std::atomic<uint64_t> counter{0};
    return counter.fetch_add(1, std::memory_order_relaxed) + 1;
}

// Pull a flow::Image out of a NodeData handle if it carries one; returns
// nullptr on any mismatch (wrong handle, wrong payload type, null data).
const Image* image_from_handle(FlowNodeDataHandle data) noexcept {
    if (!data || !flow_is_valid_handle(data)) {
        return nullptr;
    }
    auto* wrapper = flow_ffi::get_handle<NodeDataWrapper>(data);
    if (!wrapper || !wrapper->data) {
        return nullptr;
    }
    if (wrapper->data->Type() != TypeName_v<Image>) {
        return nullptr;
    }
    auto* typed = static_cast<detail::NodeData<Image>*>(wrapper->data.get());
    return &typed->Get();
}

} // namespace

extern "C" {

FLOW_FFI_EXPORT FlowNodeDataHandle flow_data_create_image_encoded(const uint8_t* bytes,
                                                                  size_t length, int32_t width,
                                                                  int32_t height) {
    FLOW_API_CALL_HANDLE({
        if (length > 0 && !flow_ffi::validate_pointer(const_cast<uint8_t*>(bytes), "bytes")) {
            return nullptr;
        }
        if (length == 0) {
            flow_ffi::ErrorManager::instance().set_error(
                FLOW_ERROR_INVALID_ARGUMENT, "Invalid argument: length must be > 0");
            return nullptr;
        }

        Image img;
        img.kind = Image::Kind::Encoded;
        img.format = Image::Format::RGBA8888; // unused for encoded; kept defaulted
        img.width = width;
        img.height = height;
        img.row_stride = 0;
        img.bytes = std::make_shared<std::vector<uint8_t>>(bytes, bytes + length);
        img.content_version = next_content_version();

        auto wrapper = NodeDataWrapper(wrap_image(std::move(img)));
        return reinterpret_cast<FlowNodeDataHandle>(
            flow_ffi::create_handle<NodeDataWrapper>(wrapper));
    });
}

FLOW_FFI_EXPORT FlowNodeDataHandle flow_data_create_image_raw(const uint8_t* bytes, int32_t width,
                                                              int32_t height,
                                                              int32_t row_stride_bytes,
                                                              FlowPixelFormat format) {
    FLOW_API_CALL_HANDLE({
        if (!flow_ffi::validate_pointer(const_cast<uint8_t*>(bytes), "bytes")) {
            return nullptr;
        }
        if (width <= 0 || height <= 0) {
            flow_ffi::ErrorManager::instance().set_error(
                FLOW_ERROR_INVALID_ARGUMENT, "Invalid argument: width and height must be > 0");
            return nullptr;
        }
        if (format != FLOW_PIXEL_FORMAT_RGBA8888) {
            flow_ffi::ErrorManager::instance().set_error(
                FLOW_ERROR_INVALID_ARGUMENT,
                "Invalid argument: only FLOW_PIXEL_FORMAT_RGBA8888 is supported in v1");
            return nullptr;
        }
        const int32_t min_stride = width * 4;
        if (row_stride_bytes < min_stride) {
            flow_ffi::ErrorManager::instance().set_error(
                FLOW_ERROR_INVALID_ARGUMENT,
                "Invalid argument: row_stride_bytes must be >= width * 4");
            return nullptr;
        }
        const std::size_t total = static_cast<std::size_t>(row_stride_bytes) *
                                  static_cast<std::size_t>(height);

        Image img;
        img.kind = Image::Kind::RawRGBA;
        img.format = Image::Format::RGBA8888;
        img.width = width;
        img.height = height;
        img.row_stride = row_stride_bytes;
        img.bytes = std::make_shared<std::vector<uint8_t>>(bytes, bytes + total);
        img.content_version = next_content_version();

        auto wrapper = NodeDataWrapper(wrap_image(std::move(img)));
        return reinterpret_cast<FlowNodeDataHandle>(
            flow_ffi::create_handle<NodeDataWrapper>(wrapper));
    });
}

FLOW_FFI_EXPORT FlowError flow_data_image_borrow(FlowNodeDataHandle data,
                                                 FlowImageDescriptor* out_desc) {
    FLOW_API_CALL({
        if (!flow_ffi::validate_handle(data, "data")) {
            return FLOW_ERROR_INVALID_HANDLE;
        }
        if (!flow_ffi::validate_pointer(out_desc, "out_desc")) {
            return FLOW_ERROR_INVALID_ARGUMENT;
        }

        auto* wrapper = flow_ffi::get_handle<NodeDataWrapper>(data);
        if (!wrapper || !wrapper->data) {
            flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_HANDLE,
                                                         "Invalid data handle");
            return FLOW_ERROR_INVALID_HANDLE;
        }
        if (wrapper->data->Type() != TypeName_v<Image>) {
            flow_ffi::ErrorManager::instance().set_error(
                FLOW_ERROR_TYPE_MISMATCH,
                std::string("Expected flow::Image, got ") + std::string(wrapper->data->Type()));
            return FLOW_ERROR_TYPE_MISMATCH;
        }

        auto* typed = static_cast<detail::NodeData<Image>*>(wrapper->data.get());
        const Image& img = typed->Get();

        out_desc->kind = (img.kind == Image::Kind::RawRGBA) ? FLOW_IMAGE_KIND_RAW_RGBA
                                                            : FLOW_IMAGE_KIND_ENCODED;
        out_desc->format = FLOW_PIXEL_FORMAT_RGBA8888; // only format supported in v1
        out_desc->width = img.width;
        out_desc->height = img.height;
        out_desc->row_stride_bytes = img.row_stride;
        out_desc->bytes = img.bytes ? img.bytes->data() : nullptr;
        out_desc->bytes_length = img.bytes ? img.bytes->size() : 0;
        out_desc->content_version = img.content_version;

        return FLOW_SUCCESS;
    });
}

FLOW_FFI_EXPORT bool flow_data_is_image(FlowNodeDataHandle data) {
    // Deliberately permissive: returns false on any null/invalid handle
    // without setting a global error, since callers use this as a cheap
    // dispatch predicate in tight loops (the Dart coalescer's hot path).
    if (!data) {
        return false;
    }
    return image_from_handle(data) != nullptr;
}

} // extern "C"
