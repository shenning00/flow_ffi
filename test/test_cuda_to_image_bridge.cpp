// test_cuda_to_image_bridge.cpp — Phase G: CudaToImage adapter node smoke test
//
// Verifies that CudaToImage::Compute() correctly copies CUDA device memory to
// a host-side flow::Image via cudaMemcpy2D.  The test is self-contained: it
// allocates device memory, fills it with a known byte pattern, constructs the
// node directly in C++, drives Compute, then checks the output.
//
// Only compiled when FLOW_FFI_ENABLE_CUDA is ON (gated by the CMakeLists.txt
// conditional).  The test binary exits 0 on success, non-zero on failure.

#include <cuda_runtime.h>

#include <flow/core/Env.hpp>
#include <flow/core/IndexableName.hpp>
#include <flow/core/NodeData.hpp>
#include <flow/core/NodeFactory.hpp>
#include <flow/core/UUID.hpp>

#include <wire/CudaDeviceImage.hpp>
#include <wire/CudaToImage.hpp>
#include <wire/Image.hpp>

#include <cstdio>
#include <cstdlib>
#include <memory>
#include <vector>

// ---------------------------------------------------------------------------
// Minimal helpers
// ---------------------------------------------------------------------------

static void cuda_check(cudaError_t err, const char* where)
{
    if (err != cudaSuccess)
    {
        std::fprintf(stderr, "CUDA error at %s: %s\n", where, cudaGetErrorString(err));
        std::exit(1);
    }
}

// ---------------------------------------------------------------------------
// Test: 16x16 RGBA8 device buffer filled with 0xAB, copied to host image
// ---------------------------------------------------------------------------

int main()
{
    // (1) Choose a small image: 16 wide x 16 tall, 4 bytes/pixel.
    constexpr int32_t W         = 16;
    constexpr int32_t H         = 16;
    constexpr int32_t ROW_BYTES = W * 4;
    constexpr std::size_t TOTAL = static_cast<std::size_t>(ROW_BYTES) * H;
    constexpr uint8_t FILL_BYTE = 0xAB;

    // (2) Allocate device memory via cudaMallocAsync on the legacy stream and
    //     wrap it in a CudaBuffer for RAII.  CudaBuffer's constructor calls
    //     cudaMallocAsync internally, so we let it own the allocation.
    auto buf = std::make_shared<flow::CudaBuffer>(TOTAL, /*device=*/0, cudaStreamLegacy);

    // Synchronize the async allocation before issuing cudaMemset.
    cuda_check(cudaStreamSynchronize(cudaStreamLegacy), "sync after alloc");

    // Fill the CudaBuffer's device memory with a known byte pattern.
    cuda_check(cudaMemset(buf->ptr(), FILL_BYTE, TOTAL), "cudaMemset(buf)");
    // Synchronize so the fill is visible before the node reads it.
    // (ready_event == nullptr in the CudaDeviceImage means the node skips
    // cudaEventSynchronize and trusts that the buffer is already settled.)
    cuda_check(cudaDeviceSynchronize(), "sync after memset");

    // (3) Construct the CudaDeviceImage wire type.
    flow::CudaDeviceImage img;
    img.buffer          = buf;
    img.width           = W;
    img.height          = H;
    img.pitch_bytes     = ROW_BYTES; // tightly packed (no device alignment padding)
    img.content_version = 1;
    img.ready_event     = nullptr; // no event; cudaDeviceSynchronize above ensures visibility

    // (4) Construct a minimal flow::Env — mirrors what flow_env_create does
    //     inside the FFI bridge (env_bridge.cpp line ~34–42).
    auto factory = std::make_shared<flow::NodeFactory>();
    flow::Settings settings;
    settings.MaxThreads = 1;
    auto env            = flow::Env::Create(factory, settings);

    // (5) Instantiate the CudaToImage node.
    flow::UUID uuid;
    flow::CudaToImage node(uuid, "cuda_to_image_test", env);

    // (6) Drive Compute by setting the input port directly, then calling
    //     InvokeCompute() synchronously on this thread.
    //     compute=false so SetInputData doesn't enqueue an async worker task.
    node.SetInputData(flow::IndexableName{"in"}, std::make_shared<flow::NodeData<flow::CudaDeviceImage>>(img),
                      /*compute=*/false);

    node.InvokeCompute();

    // (7) Read back the output port.
    auto out_data = node.GetOutputData<flow::Image>(flow::IndexableName{"out"});
    if (!out_data)
    {
        std::fprintf(stderr, "FAIL: CudaToImage produced no output on 'out' port\n");
        return 1;
    }
    const flow::Image& result = out_data->Get();

    // (8) Validate dimensions.
    if (result.width != W || result.height != H)
    {
        std::fprintf(stderr, "FAIL: expected %dx%d, got %dx%d\n", W, H, result.width, result.height);
        return 1;
    }

    // (9) Validate kind.
    if (result.kind != flow::Image::Kind::RawRGBA)
    {
        std::fprintf(stderr, "FAIL: expected kind=RawRGBA, got %d\n", static_cast<int>(result.kind));
        return 1;
    }

    // (10) Validate byte count.
    if (!result.bytes || result.bytes->size() != TOTAL)
    {
        std::fprintf(stderr, "FAIL: expected %zu bytes, got %zu\n", TOTAL, result.bytes ? result.bytes->size() : 0u);
        return 1;
    }

    // (11) Verify every byte equals the fill value.
    const auto& bytes = *result.bytes;
    for (std::size_t i = 0; i < TOTAL; ++i)
    {
        if (bytes[i] != FILL_BYTE)
        {
            std::fprintf(stderr, "FAIL: bytes[%zu] = 0x%02X, expected 0x%02X\n", i, bytes[i], FILL_BYTE);
            return 1;
        }
    }

    std::printf("PASS: CudaToImage copied %dx%d RGBA8 (0x%02X fill) correctly "
                "(%zu bytes verified)\n",
                W, H, FILL_BYTE, TOTAL);
    return 0;
}
