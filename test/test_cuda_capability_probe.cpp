// test_cuda_capability_probe.cpp — P13 smoke test
// Verifies that the CUDA capability FFI surface is callable and returns
// consistent results.  Works in both CUDA-enabled and stub builds:
//   CUDA build:    available=1 count=1 device[0]=NVIDIA GeForce RTX 4080 (or similar)
//   No-CUDA build: available=0 count=0
//
// Exit code 0 = pass; any non-zero = fail (symbol missing / crash / bad result).

#include <flow_ffi_cuda.h>
#include <cstdio>
#include <cstring>

int main() {
    bool avail = flow_ffi_cuda_available();
    int  count = flow_ffi_cuda_device_count();

    printf("available=%d count=%d", (int)avail, count);

    for (int i = 0; i < count; ++i) {
        const char* name = flow_ffi_cuda_device_name(i);
        printf(" device[%d]=%s", i, name ? name : "(null)");
    }
    printf("\n");
    fflush(stdout);

    // Consistency checks:
    // 1. If available=1, count must be >= 1.
    if (avail && count <= 0) {
        fprintf(stderr, "FAIL: available=true but device count=%d\n", count);
        return 1;
    }
    // 2. If available=0, count must be 0.
    if (!avail && count != 0) {
        fprintf(stderr, "FAIL: available=false but device count=%d\n", count);
        return 1;
    }
    // 3. Out-of-range device name must return "" not crash.
    const char* oob = flow_ffi_cuda_device_name(-1);
    if (oob == nullptr) {
        fprintf(stderr, "FAIL: flow_ffi_cuda_device_name(-1) returned nullptr\n");
        return 1;
    }
    // 4. Each valid device name must be non-null and non-empty.
    for (int i = 0; i < count; ++i) {
        const char* n = flow_ffi_cuda_device_name(i);
        if (!n || strlen(n) == 0) {
            fprintf(stderr, "FAIL: device[%d] name is null or empty\n", i);
            return 1;
        }
    }

    return 0;
}
