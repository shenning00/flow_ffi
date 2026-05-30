// texture_registrar_slot.cpp
// Holds a single opaque pointer to the platform's FlTextureRegistrar (Linux)
// or equivalent on other platforms.  Set by the Flutter plugin during
// registration; read by future CUDA/GPU nodes at draw time.
//
// No flutter_linux headers are included here — the pointer is void* to keep
// this TU free of platform SDK dependencies.  The plugin (which does include
// flutter_linux) casts to FlTextureRegistrar* before use.

#include "flow_ffi.h"

#include <atomic>
#include <cstddef>

// Single-slot atomic storage.  seq_cst ensures any GPU node that reads this
// after the plugin registration completes will see the written value without
// additional fences.
static std::atomic<void*> g_texture_registrar{nullptr};

extern "C"
{

    FLOW_FFI_EXPORT void flow_ffi_set_texture_registrar(void* registrar)
    {
        g_texture_registrar.store(registrar, std::memory_order_seq_cst);
    }

    FLOW_FFI_EXPORT void* flow_ffi_get_texture_registrar(void)
    {
        return g_texture_registrar.load(std::memory_order_seq_cst);
    }

    FLOW_FFI_EXPORT int flow_ffi_is_texture_registrar_bound(void)
    {
        return g_texture_registrar.load(std::memory_order_seq_cst) != nullptr ? 1 : 0;
    }

} // extern "C"
