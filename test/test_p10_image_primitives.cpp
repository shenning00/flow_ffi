#include "flow_ffi.h"

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include <gtest/gtest.h>

// test_p10_image_primitives.cpp — eight acceptance cases for P10 per
// FLOW_RUN.html §B.9.  Covers create / borrow round-trips, type-mismatch,
// double-borrow safety, lifetime via destroy, content_version monotonicity,
// and PreviewNode registration in the factory.

namespace {

class P10ImagePrimitivesTest : public ::testing::Test {
  protected:
    void SetUp() override { flow_clear_error(); }
    void TearDown() override { flow_clear_error(); }

    // Helper: synthesise a small RGBA8 raster (tightly packed) so each test
    // can construct an image without depending on a real PNG/JPEG codec.
    static std::vector<uint8_t> make_rgba(int w, int h, uint8_t seed = 0) {
        std::vector<uint8_t> buf(static_cast<std::size_t>(w) * h * 4);
        for (std::size_t i = 0; i < buf.size(); ++i) {
            buf[i] = static_cast<uint8_t>((i + seed) & 0xFF);
        }
        return buf;
    }
};

// (1) Round-trip an encoded blob: create → borrow → bytes/length match.
TEST_F(P10ImagePrimitivesTest, CreateEncodedRoundTrip) {
    const uint8_t bytes[] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A, 1, 2, 3, 4};
    FlowNodeDataHandle h =
        flow_data_create_image_encoded(bytes, sizeof(bytes), /*width=*/-1, /*height=*/-1);
    ASSERT_NE(h, nullptr);
    ASSERT_TRUE(flow_data_is_image(h));

    FlowImageDescriptor d{};
    ASSERT_EQ(flow_data_image_borrow(h, &d), FLOW_SUCCESS);
    EXPECT_EQ(d.kind, FLOW_IMAGE_KIND_ENCODED);
    EXPECT_EQ(d.width, -1);
    EXPECT_EQ(d.height, -1);
    EXPECT_EQ(d.bytes_length, sizeof(bytes));
    ASSERT_NE(d.bytes, nullptr);
    EXPECT_EQ(std::memcmp(d.bytes, bytes, sizeof(bytes)), 0);

    flow_data_destroy(h);
}

// (2) Round-trip a raw RGBA buffer including a non-tight stride.
TEST_F(P10ImagePrimitivesTest, CreateRawRoundTripWithStride) {
    constexpr int W = 8, H = 4;
    constexpr int STRIDE = W * 4 + 8; // 8 bytes of padding per row
    std::vector<uint8_t> buf(static_cast<std::size_t>(STRIDE) * H);
    for (std::size_t i = 0; i < buf.size(); ++i) {
        buf[i] = static_cast<uint8_t>(i & 0xFF);
    }

    FlowNodeDataHandle h =
        flow_data_create_image_raw(buf.data(), W, H, STRIDE, FLOW_PIXEL_FORMAT_RGBA8888);
    ASSERT_NE(h, nullptr);

    FlowImageDescriptor d{};
    ASSERT_EQ(flow_data_image_borrow(h, &d), FLOW_SUCCESS);
    EXPECT_EQ(d.kind, FLOW_IMAGE_KIND_RAW_RGBA);
    EXPECT_EQ(d.format, FLOW_PIXEL_FORMAT_RGBA8888);
    EXPECT_EQ(d.width, W);
    EXPECT_EQ(d.height, H);
    EXPECT_EQ(d.row_stride_bytes, STRIDE);
    EXPECT_EQ(d.bytes_length, buf.size());
    EXPECT_EQ(std::memcmp(d.bytes, buf.data(), buf.size()), 0);

    flow_data_destroy(h);
}

// (3) Borrow exposes a view, not a copy: descriptor.bytes points inside the
//     handle's storage and is stable until destroy is called.  We compare
//     against the original input bytes for byte-equality.
TEST_F(P10ImagePrimitivesTest, BorrowYieldsReadOnlyView) {
    auto rgba = make_rgba(4, 4);
    FlowNodeDataHandle h = flow_data_create_image_raw(
        rgba.data(), 4, 4, 16, FLOW_PIXEL_FORMAT_RGBA8888);
    ASSERT_NE(h, nullptr);

    FlowImageDescriptor d{};
    ASSERT_EQ(flow_data_image_borrow(h, &d), FLOW_SUCCESS);
    ASSERT_NE(d.bytes, nullptr);
    EXPECT_EQ(d.bytes_length, rgba.size());
    EXPECT_EQ(std::memcmp(d.bytes, rgba.data(), rgba.size()), 0);

    flow_data_destroy(h);
}

// (4) Type-mismatch: borrow on a non-image handle returns TYPE_MISMATCH
//     and is_image returns false.
TEST_F(P10ImagePrimitivesTest, BorrowOnNonImageReturnsTypeMismatch) {
    FlowNodeDataHandle prim = flow_data_create_int(42);
    ASSERT_NE(prim, nullptr);
    EXPECT_FALSE(flow_data_is_image(prim));

    FlowImageDescriptor d{};
    EXPECT_EQ(flow_data_image_borrow(prim, &d), FLOW_ERROR_TYPE_MISMATCH);

    flow_data_destroy(prim);
}

// (5) Double-borrow safety: two consecutive borrows yield identical
//     descriptors and do not mutate state.
TEST_F(P10ImagePrimitivesTest, DoubleBorrowIsIdempotent) {
    auto rgba = make_rgba(2, 2);
    FlowNodeDataHandle h = flow_data_create_image_raw(
        rgba.data(), 2, 2, 8, FLOW_PIXEL_FORMAT_RGBA8888);
    ASSERT_NE(h, nullptr);

    FlowImageDescriptor d1{};
    FlowImageDescriptor d2{};
    ASSERT_EQ(flow_data_image_borrow(h, &d1), FLOW_SUCCESS);
    ASSERT_EQ(flow_data_image_borrow(h, &d2), FLOW_SUCCESS);

    EXPECT_EQ(d1.kind, d2.kind);
    EXPECT_EQ(d1.format, d2.format);
    EXPECT_EQ(d1.width, d2.width);
    EXPECT_EQ(d1.height, d2.height);
    EXPECT_EQ(d1.row_stride_bytes, d2.row_stride_bytes);
    EXPECT_EQ(d1.bytes_length, d2.bytes_length);
    EXPECT_EQ(d1.content_version, d2.content_version);
    EXPECT_EQ(d1.bytes, d2.bytes); // same view, same pointer

    flow_data_destroy(h);
}

// (6) Lifetime: borrow → snapshot → destroy.  After destroy, the borrowed
//     pointer is dangling — re-reading it is UB and ASAN catches it.  This
//     test verifies the *handle* is gone (borrow returns INVALID_HANDLE on
//     a fresh attempt after destroy).  An ASAN build will additionally
//     catch any read-after-free of the saved pointer.
TEST_F(P10ImagePrimitivesTest, DestroyInvalidatesHandle) {
    auto rgba = make_rgba(2, 2, /*seed=*/7);
    FlowNodeDataHandle h = flow_data_create_image_raw(
        rgba.data(), 2, 2, 8, FLOW_PIXEL_FORMAT_RGBA8888);
    ASSERT_NE(h, nullptr);

    FlowImageDescriptor d{};
    ASSERT_EQ(flow_data_image_borrow(h, &d), FLOW_SUCCESS);
    EXPECT_EQ(std::memcmp(d.bytes, rgba.data(), rgba.size()), 0);

    flow_data_destroy(h);

    // The handle is no longer valid: a fresh borrow attempt is rejected.
    FlowImageDescriptor d2{};
    EXPECT_EQ(flow_data_image_borrow(h, &d2), FLOW_ERROR_INVALID_HANDLE);
    EXPECT_FALSE(flow_data_is_image(h));
}

// (7) content_version is monotonic across creates within the same process.
TEST_F(P10ImagePrimitivesTest, ContentVersionIsMonotonic) {
    FlowImageDescriptor d_prev{};
    uint64_t prev = 0;

    for (int i = 0; i < 5; ++i) {
        auto rgba = make_rgba(2, 2, /*seed=*/static_cast<uint8_t>(i));
        FlowNodeDataHandle h = flow_data_create_image_raw(
            rgba.data(), 2, 2, 8, FLOW_PIXEL_FORMAT_RGBA8888);
        ASSERT_NE(h, nullptr);

        FlowImageDescriptor d{};
        ASSERT_EQ(flow_data_image_borrow(h, &d), FLOW_SUCCESS);
        EXPECT_GT(d.content_version, prev);
        prev = d.content_version;
        d_prev = d;

        flow_data_destroy(h);
    }
    (void)d_prev; // suppress unused-warning if any
}

// (8) PreviewNode is registered against the env's factory under category
//     "Editor".  Its class name (TypeName_v<PreviewNode>) appears in
//     flow_factory_get_node_classes, and its friendly name is "Preview".
TEST_F(P10ImagePrimitivesTest, PreviewNodeIsRegistered) {
    FlowEnvHandle env = flow_env_create(1);
    ASSERT_NE(env, nullptr);

    FlowNodeFactoryHandle factory = flow_env_get_factory(env);
    ASSERT_NE(factory, nullptr);

    char** classes = nullptr;
    std::size_t count = 0;
    ASSERT_EQ(flow_factory_get_node_classes(factory, "Editor", &classes, &count), FLOW_SUCCESS);
    ASSERT_GT(count, 0u);

    bool found_preview = false;
    std::string preview_class_name;
    for (std::size_t i = 0; i < count; ++i) {
        ASSERT_NE(classes[i], nullptr);
        std::string name = classes[i];
        // The class name carries the full C++ namespace; match by suffix.
        if (name.find("PreviewNode") != std::string::npos) {
            found_preview = true;
            preview_class_name = name;
        }
    }
    flow_free_string_array(classes, count);

    EXPECT_TRUE(found_preview) << "Expected a class name containing 'PreviewNode' "
                                  "under category 'Editor'";

    if (found_preview) {
        const char* friendly = flow_factory_get_friendly_name(factory, preview_class_name.c_str());
        ASSERT_NE(friendly, nullptr);
        EXPECT_STREQ(friendly, "Preview");
    }

    flow_env_destroy(env);
}

} // namespace
