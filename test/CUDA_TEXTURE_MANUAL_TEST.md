# Phase D — CUDA/Flutter Texture Interop: Manual Test Procedure

## Prerequisites

- Linux x86_64 with an NVIDIA GPU (RTX 4000 series or newer recommended)
- CUDA 12.x driver + toolkit (`nvcc --version` succeeds)
- Flutter SDK ≥ 3.10 with Linux desktop support (`flutter devices` shows linux)
- GTK3 dev libs (`pkg-config --modversion gtk+-3.0` succeeds)
- libepoxy dev (`pkg-config --exists epoxy` succeeds)
- A physical display or a virtual framebuffer (`Xvfb`) with hardware GL

## Phase E note

Phase D provides the texture allocation and write machinery.  Visible output
in the editor (a CudaPreview node rendering GPU-resident images) requires Phase E
(FlowFlutterCudaPreview) which is not yet implemented.  The manual tests below
verify the interop layer in isolation and via a minimal Dart + FFI smoke test.

---

## Step 1: Build libflow_ffi.so with CUDA enabled

```sh
cd /home/shenning/Development/flow/flow_ffi
cmake -S . -B build_cuda \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DFLOW_FFI_ENABLE_CUDA=ON
cmake --build build_cuda -j$(nproc)
```

Expected: `build_cuda/libflow_ffi.so` built without errors.

Verify the 4 interop symbols are no longer stubs:
```sh
nm -D build_cuda/libflow_ffi.so | grep -E \
    "flow_ffi_register_cuda|flow_ffi_unregister_cuda|flow_ffi_cuda_write_into|flow_ffi_signal_frame"
```
All four should be `T` (defined text symbols).

Verify GL/CUDA interop symbols are referenced:
```sh
nm -u build_cuda/libflow_ffi.so | grep -E "cudaGraphics|glGenTextures"
```
These are `U` (undefined, resolved at runtime from libcudart / libGL).

## Step 2: Run Phase D unit tests (headless)

```sh
cd /home/shenning/Development/flow/flow_ffi/build_cuda
ctest --output-on-failure -R "CudaTextureInterop"
```

All API-surface tests (A1–A13) should pass.

The GPU write test (B1) will:
- SKIP if no CUDA device is present (shouldn't happen on this machine)
- SKIP if EGL PBuffer creation fails (headless w/o virtual framebuffer)
- PASS if a real GL context is available

To force a virtual framebuffer for the GPU test:
```sh
Xvfb :99 -screen 0 1024x768x24 -ac +extension GLX &
DISPLAY=:99 ctest --output-on-failure -R GpuWriteVerification
```

## Step 3: Build the Flutter example app with CUDA plugin

```sh
# Set FLOW_FFI_BUILD_DIR to the CUDA build so the plugin finds the right .so
export FLOW_FFI_BUILD_DIR=/home/shenning/Development/flow/flow_ffi/build_cuda

cd /home/shenning/Development/flow/flutter_fl_nodes/examples/fl_nodes_example
flutter build linux --debug \
    --dart-define=FLOW_FFI_ENABLE_CUDA=1 \
    -- -DFLOW_FFI_ENABLE_CUDA=ON
```

Expected: `✓ Built build/linux/x64/debug/bundle/fl_nodes_example`

Verify CUDA-aware plugin .so:
```sh
nm -D build/linux/x64/debug/bundle/lib/libflow_ffi_flutter_plugin.so \
    | grep "flow_ffi_cuda_texture_bridge_register"
```
Should show `T flow_ffi_cuda_texture_bridge_register`.

## Step 4: Run the app and verify interop bridge registration

```sh
cd examples/fl_nodes_example
flutter run -d linux --debug
```

In the console output, you should see:
```
[flow_ffi_flutter] FlTextureRegistrar captured successfully (ptr=0x...).
Phase D CUDA texture registration is ready.
[flow_ffi cuda_bridge] FlowTextureOps injected into libflow_ffi.so
```

Both messages must appear before the first frame renders.

## Step 5: Dart FFI smoke test (texture registration round-trip)

Write and run a minimal Dart integration test that:
1. Calls `flow_ffi_is_texture_registrar_bound()` — expect 1
2. Calls `flow_ffi_is_texture_ops_bound()` — expect 1
3. Calls `flow_ffi_register_cuda_flutter_texture(64, 64, handle_ptr, id_ptr)`
   — expect FLOW_SUCCESS, handle != 0, texture_id > 0
4. Instantiates a `Texture(textureId: id)` widget — expect it to render
   (no red/error screen)
5. Calls `flow_ffi_signal_frame_available(texture_id)` — expect no crash
6. Calls `flow_ffi_unregister_cuda_flutter_texture(handle)` — expect FLOW_SUCCESS

A placeholder integration test scaffold lives at:
  `examples/fl_nodes_example/test/cuda_texture_interop_test.dart`
(to be added in Phase E along with the CudaPreview Dart widget).

## Step 6: Verify known-color GPU image appears in the editor (Phase E gate)

This step requires Phase E (FlowFlutterCudaPreview node) which is not yet
implemented.  Once Phase E lands:

1. Open the editor and add a `CudaPreview` node to the graph.
2. Connect a `CudaSolidColor` source node (also Phase E) configured to
   fill the frame with 100% red (R=255 G=0 B=0 A=255).
3. Advance one frame.
4. The `Texture` widget in the node body should display a solid red rectangle
   matching the node's dimensions.

## Known limitations / blockers

- `cudaGraphicsGLRegisterImage` requires that the GL texture was created in a
  context that shares state with the current CUDA GL context.  On Flutter Linux
  (GTK3), the `FlView`'s GdkGLContext is shared with the Flutter compositor GL
  context; `cuda_texture_bridge.cc` uses `ensure_gl_context_current()` to
  obtain a context before calling `glGenTextures`.  In early-registration
  scenarios (before `FlView` is shown), the GL context may not yet be
  current.  If this occurs, `glGenTextures` returns 0 and registration returns
  `FLOW_ERROR_UNKNOWN`.  Retry registration after the first frame has rendered.

- The EGL PBuffer path in the GPU unit test (`GpuWriteVerification`) creates
  an independent GL context.  CUDA GL interop requires the GL texture to have
  been created in a context that matches the current CUDA device's GL context.
  An EGL PBuffer context may satisfy this requirement on Mesa/NVIDIA, but it
  is not guaranteed.  If `cudaGraphicsGLRegisterImage` fails in the test with
  `cudaErrorUnknown`, this indicates a driver-level restriction, not a bug in
  the Phase D implementation.

- On Wayland-only setups without XWayland, EGL display enumeration may fail.
  Set `DISPLAY=:0` or run with `Xvfb` as described in Step 2.
