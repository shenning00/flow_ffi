// wire::RegisterAllNodes — single registration entry point called by
// the FFI/frontend binding during environment setup.  See Register.hpp for
// the rationale; see flow_ffi/src/env_bridge.cpp for an example caller.

#include <flow/core/Env.hpp>
#include <flow/core/NodeFactory.hpp>

#include <wire/DisplayValueNode.hpp>
#include <wire/FlowFlutterTextureSink.hpp>
#include <wire/FlowOutputImage.hpp>
#include <wire/ImageOpenNode.hpp>
#include <wire/PreviewNode.hpp>
#include <wire/Register.hpp>

#ifdef FLOW_INTERWORK_WITH_OPENCV
#include <wire/opencv/CvMatToImage.hpp>
#include <wire/opencv/ImageToCvMat.hpp>
#endif

#ifdef WIRE_WITH_CUDA
#include <wire/CudaToImage.hpp>
#include <wire/FlowCudaOutputImage.hpp>
#include <wire/FlowFlutterCudaPreview.hpp>
#endif

namespace wire
{

void RegisterAllNodes(flow::Env& env)
{
    auto factory = env.GetFactory();
    if (!factory)
    {
        // Nothing to do — env without a factory cannot host nodes.  Defensive
        // check; flow_env_create always provides one today.
        return;
    }

    // Always-present wire-type nodes.  The category "Editor" matches the
    // previous flow_ffi behavior so the right-click menu layout is unchanged.
    factory->RegisterNodeClass<flow::ImageOpenNode>("Editor", "ImageOpen");
    factory->RegisterNodeClass<flow::PreviewNode>("Editor", "Preview");

    // Generic primitive-value display sink.  Paired with FlowBridge's
    // generalized _wireDisplayField helper on the Dart side: any node with a
    // "displayed" field has its input ports auto-subscribed and the live
    // value stringified into the field.  Drag any primitive output here to
    // see the value in the node body.
    factory->RegisterNodeClass<flow::DisplayValueNode>("Display", "DisplayValue");

    // P12: CPU-host→Flutter texture sink.  Unconditional — available on all
    // builds; gated in the Dart palette by the FlowGpuTextureOps vtable being
    // bound (flow_ffi_gpu_texture_sink_available).
    factory->RegisterNodeClass<flow::FlowFlutterTextureSink>("Editor", "TextureSink");

    // P15: CPU-host image output sink (channel label, no compute).
    factory->RegisterNodeClass<flow::FlowOutputImage>("Editor", "OutputImage");

#ifdef FLOW_INTERWORK_WITH_OPENCV
    // Adapter nodes compiled in only when OpenCV was available at configure
    // time.  Category "Adapters" keeps them visually distinct from the
    // opencv_nodes module's per-operation entries.
    factory->RegisterNodeClass<flow::ImageToCvMat>("Adapters", "ImageToCvMat");
    factory->RegisterNodeClass<flow::CvMatToImage>("Adapters", "CvMatToImage");
#endif

#ifdef WIRE_WITH_CUDA
    // CUDA adapter node: converts a GPU-resident CudaDeviceImage to a host-side
    // flow::Image via a synchronous device→host copy.  Registered only when
    // CUDA support was enabled at configure time (FLOW_FFI_ENABLE_CUDA=ON).
    factory->RegisterNodeClass<flow::CudaToImage>("Adapters", "CudaToImage");

    // CUDA→Flutter texture sink node: feeds GPU-resident frames directly into
    // Flutter's TextureRegistry via CUDA/GL interop, bypassing the CPU copy
    // path.  Category "Editor" groups it with the regular PreviewNode in the
    // right-click menu.
    factory->RegisterNodeClass<flow::FlowFlutterCudaPreview>("Editor", "CudaPreview");

    // P15: CUDA image output sink (channel label, texture_id output).
    factory->RegisterNodeClass<flow::FlowCudaOutputImage>("Editor", "CudaOutputImage");
#endif
}

} // namespace wire
