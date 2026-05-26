// flow_ffi/dart_package/lib/src/capabilities.dart
//
// Dart-side CUDA capability surface (Phase F / §6.4).
//
// Call [FlowFfiCapabilities.probe()] once at engine init to determine whether
// the host supports the CUDA image path.  The result is stored on [FlowBridge]
// and used to gate the CudaPreview entry in the right-click menu.
//
// The three native symbols are always present in libflow_ffi.so regardless of
// whether it was compiled with CUDA.  When built without CUDA they return
// false / 0 / "" — so probe() is unconditionally safe.

import 'package:flow_ffi/src/ffi/bindings.dart' show flowCore;

/// Immutable snapshot of the host's CUDA (and future GPU) capabilities,
/// captured once at bridge initialisation via [FlowFfiCapabilities.probe].
///
/// Stored as [FlowBridge.capabilities]; consumers gate GPU-specific UI on
/// [hasCudaImagePath] and [hasGpuTextureSink].
class FlowFfiCapabilities {
  /// True when libflow_ffi.so was compiled with CUDA *and* at least one
  /// CUDA device was found on this machine.
  final bool hasCudaImagePath;

  /// Display names of every enumerated CUDA device, in device-index order.
  /// Empty when [hasCudaImagePath] is false.
  final List<String> cudaDeviceNames;

  /// P12: True when the GPU texture ops vtable has been injected by the Flutter
  /// plugin (i.e. the Linux plugin was loaded and texture_sink_bridge.cc
  /// registered its callbacks).  This is independent of CUDA — it reflects
  /// the non-CUDA CPU→GL texture path availability.
  final bool hasGpuTextureSink;

  const FlowFfiCapabilities({
    required this.hasCudaImagePath,
    required this.cudaDeviceNames,
    this.hasGpuTextureSink = false,
  });

  /// Convenience constant for non-GPU hosts or test doubles.
  static const FlowFfiCapabilities none = FlowFfiCapabilities(
    hasCudaImagePath: false,
    cudaDeviceNames: [],
    hasGpuTextureSink: false,
  );

  /// Query the native library for GPU capabilities.
  ///
  /// Safe to call before [FlowBridge.initialize] — the symbols are in
  /// libflow_ffi.so and do not require a live flow::Environment.
  static FlowFfiCapabilities probe() {
    // P12: probe GPU texture sink availability unconditionally.
    final hasGpuTexture = flowCore.flow_ffi_gpu_texture_sink_available();

    final hasCuda = flowCore.flow_ffi_cuda_available();
    if (!hasCuda) {
      return FlowFfiCapabilities(
        hasCudaImagePath: false,
        cudaDeviceNames: const [],
        hasGpuTextureSink: hasGpuTexture,
      );
    }
    final count = flowCore.flow_ffi_cuda_device_count();
    final names = <String>[];
    for (int i = 0; i < count; i++) {
      names.add(flowCore.flow_ffi_cuda_device_name(i));
    }
    return FlowFfiCapabilities(
      hasCudaImagePath: true,
      cudaDeviceNames: List.unmodifiable(names),
      hasGpuTextureSink: hasGpuTexture,
    );
  }

  @override
  String toString() => 'FlowFfiCapabilities('
      'hasCudaImagePath: $hasCudaImagePath, '
      'devices: $cudaDeviceNames, '
      'hasGpuTextureSink: $hasGpuTextureSink)';
}
