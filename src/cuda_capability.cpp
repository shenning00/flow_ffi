// cuda_capability.cpp — CUDA capability probe
// Compiled in BOTH modes (FLOW_FFI_HAS_CUDA=1 and =0).
// When FLOW_FFI_HAS_CUDA=0 all three functions are stubs returning false/0/"".
// See §6.3 of FLOW_RUN_CUDA_IMAGES.html for the design rationale.

#include "flow_ffi_cuda.h"

#if FLOW_FFI_HAS_CUDA
#include <algorithm>
#include <cstdio>
#include <cuda_runtime.h>

static int g_cuda_device_count = -1;     // -1 = not yet probed
static char g_cuda_device_names[8][256]; // small fixed buffer, up to 8 devices

static void probe_once()
{
    if (g_cuda_device_count != -1) return;
    int n           = 0;
    cudaError_t err = cudaGetDeviceCount(&n);
    if (err != cudaSuccess)
    {
        g_cuda_device_count = 0;
        return;
    }
    g_cuda_device_count = std::min(n, 8);
    for (int i = 0; i < g_cuda_device_count; ++i)
    {
        cudaDeviceProp p;
        cudaGetDeviceProperties(&p, i);
        std::snprintf(g_cuda_device_names[i], 256, "%s", p.name);
    }
}

extern "C" bool flow_ffi_cuda_available()
{
    probe_once();
    return g_cuda_device_count > 0;
}
extern "C" int flow_ffi_cuda_device_count()
{
    probe_once();
    return g_cuda_device_count;
}
extern "C" const char* flow_ffi_cuda_device_name(int i)
{
    probe_once();
    return (i >= 0 && i < g_cuda_device_count) ? g_cuda_device_names[i] : "";
}

#else // FLOW_FFI_HAS_CUDA == 0 — stub TU

extern "C" bool flow_ffi_cuda_available() { return false; }
extern "C" int flow_ffi_cuda_device_count() { return 0; }
extern "C" const char* flow_ffi_cuda_device_name(int) { return ""; }

#endif
