#ifndef FLOW_FFI_H
#define FLOW_FFI_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Export macro for shared library
#ifndef FLOW_FFI_EXPORT
#    ifdef _WIN32
#        define FLOW_FFI_EXPORT __declspec(dllexport)
#    else
#        define FLOW_FFI_EXPORT __attribute__((visibility("default")))
#    endif
#endif

// Error codes
typedef enum FlowError {
    FLOW_SUCCESS = 0,
    FLOW_ERROR_INVALID_HANDLE = -1,
    FLOW_ERROR_INVALID_ARGUMENT = -2,
    FLOW_ERROR_NODE_NOT_FOUND = -3,
    FLOW_ERROR_PORT_NOT_FOUND = -4,
    FLOW_ERROR_CONNECTION_FAILED = -5,
    FLOW_ERROR_MODULE_LOAD_FAILED = -6,
    FLOW_ERROR_COMPUTATION_FAILED = -7,
    FLOW_ERROR_OUT_OF_MEMORY = -8,
    FLOW_ERROR_TYPE_MISMATCH = -9,
    FLOW_ERROR_NOT_IMPLEMENTED = -10,
    FLOW_ERROR_UNKNOWN = -999
} FlowError;

// Opaque handle types
typedef struct FlowGraph* FlowGraphHandle;
typedef struct FlowNode* FlowNodeHandle;
typedef struct FlowConnection* FlowConnectionHandle;
typedef struct FlowEnv* FlowEnvHandle;
typedef struct FlowNodeFactory* FlowNodeFactoryHandle;
typedef struct FlowModule* FlowModuleHandle;
typedef struct FlowNodeData* FlowNodeDataHandle;

// Result structure for operations that may fail
typedef struct FlowResult {
    FlowError error;
    void* data;
} FlowResult;

// String result for functions returning strings
typedef struct FlowStringResult {
    FlowError error;
    const char* data;
} FlowStringResult;

// Connection information for graph connection queries
typedef struct FlowConnectionInfo {
    const char* id;
    const char* source_node_id;
    const char* source_port_key;
    const char* target_node_id;
    const char* target_port_key;
} FlowConnectionInfo;

// ============================================================================
// Error Handling
// ============================================================================

// Get the last error message
FLOW_FFI_EXPORT const char* flow_get_last_error(void);

// Get the last error code (FlowError enum value, returned as int for a stable C ABI)
FLOW_FFI_EXPORT int flow_get_last_error_code(void);

// Clear the last error
FLOW_FFI_EXPORT void flow_clear_error(void);

// Set custom error message
FLOW_FFI_EXPORT void flow_set_error(FlowError code, const char* message);

// ============================================================================
// Handle Management
// ============================================================================

// Validate if a handle is valid
FLOW_FFI_EXPORT bool flow_is_valid_handle(void* handle);

// Reference counting for handles
FLOW_FFI_EXPORT void flow_retain_handle(void* handle);
FLOW_FFI_EXPORT void flow_release_handle(void* handle);
FLOW_FFI_EXPORT int32_t flow_get_ref_count(void* handle);

// ============================================================================
// Environment Management
// ============================================================================

// Create a new environment with specified thread count
FLOW_FFI_EXPORT FlowEnvHandle flow_env_create(int32_t max_threads);

// Destroy an environment
FLOW_FFI_EXPORT void flow_env_destroy(FlowEnvHandle env);

// Get the node factory from environment
FLOW_FFI_EXPORT FlowNodeFactoryHandle flow_env_get_factory(FlowEnvHandle env);

// Wait for all tasks to complete
FLOW_FFI_EXPORT FlowError flow_env_wait(FlowEnvHandle env);

/**
 * Post a SetInputData call to the env's worker thread pool.
 *
 * The worker takes the node lock and calls SetInputData(port_key, data,
 * compute=true). Returns immediately on the calling thread; the caller may
 * release `data` (FFI handle) the moment this returns — the underlying
 * SharedNode and SharedNodeData are ref-counted into the worker lambda by
 * shared_ptr copy.
 *
 * Mirrors flow-ui's NodeView::on_input pattern. P1 of FLOW_RUN.html §10.11.
 *
 * @return FLOW_SUCCESS on accept; FLOW_ERROR_INVALID_HANDLE on a bad env/
 *         node/data handle; FLOW_ERROR_INVALID_ARGUMENT on null/empty
 *         port_key.
 */
FLOW_FFI_EXPORT FlowError flow_env_add_task_set_input_data(
    FlowEnvHandle      env,
    FlowNodeHandle     node,
    const char*        port_key,
    FlowNodeDataHandle data);

// Get system environment variable
FLOW_FFI_EXPORT const char* flow_env_get_var(FlowEnvHandle env, const char* name);

// ============================================================================
// Graph Management
// ============================================================================

// Create a new graph
FLOW_FFI_EXPORT FlowGraphHandle flow_graph_create(FlowEnvHandle env);

// Destroy a graph
FLOW_FFI_EXPORT void flow_graph_destroy(FlowGraphHandle graph);

// Add a node to the graph
FLOW_FFI_EXPORT FlowNodeHandle flow_graph_add_node(FlowGraphHandle graph, const char* class_id,
                                                   const char* name);

// Add a node to the graph with an explicit UUID.
// If uuid is NULL or empty a fresh UUID is generated (identical to flow_graph_add_node).
// Returns nullptr and sets FLOW_ERROR_INVALID_ARGUMENT if uuid is non-empty but malformed.
FLOW_FFI_EXPORT FlowNodeHandle flow_graph_add_node_with_uuid(FlowGraphHandle graph,
                                                              const char* class_id,
                                                              const char* uuid,
                                                              const char* name);

// Remove a node from the graph
FLOW_FFI_EXPORT FlowError flow_graph_remove_node(FlowGraphHandle graph, const char* node_id);

// Get a node by ID
FLOW_FFI_EXPORT FlowNodeHandle flow_graph_get_node(FlowGraphHandle graph, const char* node_id);

// Get all nodes
FLOW_FFI_EXPORT FlowError flow_graph_get_nodes(FlowGraphHandle graph, FlowNodeHandle** nodes,
                                               size_t* count);

// Connect two nodes
FLOW_FFI_EXPORT FlowConnectionHandle flow_graph_connect_nodes(FlowGraphHandle graph,
                                                              const char* source_id,
                                                              const char* source_port,
                                                              const char* target_id,
                                                              const char* target_port);

// Disconnect nodes
FLOW_FFI_EXPORT FlowError flow_graph_disconnect_nodes(FlowGraphHandle graph,
                                                      const char* connection_id);

// Get all connections in the graph
FLOW_FFI_EXPORT FlowError flow_graph_get_connections(FlowGraphHandle graph,
                                                     FlowConnectionInfo** connections,
                                                     size_t* count);

// Check if two nodes can be connected
FLOW_FFI_EXPORT bool flow_graph_can_connect(FlowGraphHandle graph, const char* source_id,
                                            const char* source_port, const char* target_id,
                                            const char* target_port);

// Run the graph
FLOW_FFI_EXPORT FlowError flow_graph_run(FlowGraphHandle graph);

// Clear all nodes and connections
FLOW_FFI_EXPORT FlowError flow_graph_clear(FlowGraphHandle graph);

// Serialization
FLOW_FFI_EXPORT char* flow_graph_save_to_json(FlowGraphHandle graph);
FLOW_FFI_EXPORT FlowError flow_graph_load_from_json(FlowGraphHandle graph, const char* json);

// Node serialization
FLOW_FFI_EXPORT char* flow_node_save_to_json(FlowNodeHandle node);
FLOW_FFI_EXPORT FlowError flow_node_load_from_json(FlowNodeHandle node, const char* json_str);

// ============================================================================
// Node Management
// ============================================================================

// Get node properties
FLOW_FFI_EXPORT const char* flow_node_get_id(FlowNodeHandle node);
FLOW_FFI_EXPORT const char* flow_node_get_name(FlowNodeHandle node);
FLOW_FFI_EXPORT const char* flow_node_get_class(FlowNodeHandle node);

// Set node name
FLOW_FFI_EXPORT FlowError flow_node_set_name(FlowNodeHandle node, const char* name);

// Port data management
FLOW_FFI_EXPORT FlowError flow_node_set_input_data(FlowNodeHandle node, const char* port_key,
                                                   FlowNodeDataHandle data);

FLOW_FFI_EXPORT FlowNodeDataHandle flow_node_get_input_data(FlowNodeHandle node,
                                                            const char* port_key);

FLOW_FFI_EXPORT FlowNodeDataHandle flow_node_get_output_data(FlowNodeHandle node,
                                                             const char* port_key);

FLOW_FFI_EXPORT FlowError flow_node_clear_input_data(FlowNodeHandle node, const char* port_key);

FLOW_FFI_EXPORT FlowError flow_node_clear_output_data(FlowNodeHandle node, const char* port_key);

// Computation
FLOW_FFI_EXPORT FlowError flow_node_invoke_compute(FlowNodeHandle node);
FLOW_FFI_EXPORT bool flow_node_validate_required_inputs(FlowNodeHandle node);

// Lifecycle (Start/Stop hooks).  Mirrors flow-core's Node::Start() and
// Node::Stop().  The framework calls Stop() automatically inside
// flow_graph_remove_node; Start() is explicitly opt-in by the embedder
// (flow-ui's convention; see FLOW_RUN.html §10.10).  Default no-op
// implementations always return FLOW_SUCCESS.  Double-call is safe.
FLOW_FFI_EXPORT FlowError flow_node_start(FlowNodeHandle node);
FLOW_FFI_EXPORT FlowError flow_node_stop(FlowNodeHandle node);

// Connection status
FLOW_FFI_EXPORT bool flow_node_has_connected_inputs(FlowNodeHandle node);
FLOW_FFI_EXPORT bool flow_node_has_connected_outputs(FlowNodeHandle node);

// Port introspection
FLOW_FFI_EXPORT FlowError flow_node_get_input_port_keys(FlowNodeHandle node, char*** port_keys,
                                                        size_t* count);

FLOW_FFI_EXPORT FlowError flow_node_get_output_port_keys(FlowNodeHandle node, char*** port_keys,
                                                         size_t* count);

// Get port type information
FLOW_FFI_EXPORT const char* flow_node_get_input_port_type(FlowNodeHandle node,
                                                          const char* port_key);

FLOW_FFI_EXPORT const char* flow_node_get_output_port_type(FlowNodeHandle node,
                                                           const char* port_key);

// Get port description/caption
FLOW_FFI_EXPORT const char*
flow_node_get_port_description(FlowNodeHandle node, const char* port_key, bool is_input_port);

// ============================================================================
// Port Metadata (for Optional Values)
// ============================================================================

// Port metadata structure for UI integration
// The interworking_value_json field contains JSON with type and default value
// Format: {"type":"string|integer|float|boolean|none","value":"<default_value>"}
// Examples:
//   {"type":"string","value":"/home/user/file.png"}
//   {"type":"integer","value":"640"}
//   {"type":"float","value":"2.5"}
//   {"type":"boolean","value":"true"}
//   {"type":"none"}  // Complex types, not editable
typedef struct FlowPortMetadata {
    const char* key;                     // Port identifier
    const char* interworking_value_json; // JSON string with type and value
    bool has_default;                    // Whether default value exists
} FlowPortMetadata;

// Get port metadata for a specific port
FLOW_FFI_EXPORT FlowError flow_node_get_port_metadata(FlowNodeHandle node, const char* port_key,
                                                      FlowPortMetadata* metadata);

// Get metadata for all input ports
FLOW_FFI_EXPORT FlowError flow_node_get_input_ports_metadata(FlowNodeHandle node,
                                                             FlowPortMetadata** metadata_array,
                                                             size_t* count);

// Free port metadata array
FLOW_FFI_EXPORT void flow_free_port_metadata_array(FlowPortMetadata* metadata_array, size_t count);

// Free single port metadata
FLOW_FFI_EXPORT void flow_free_port_metadata(FlowPortMetadata* metadata);

// ============================================================================
// Connection Management
// ============================================================================

// Get connection properties
FLOW_FFI_EXPORT const char* flow_connection_get_id(FlowConnectionHandle conn);
FLOW_FFI_EXPORT const char* flow_connection_get_start_node_id(FlowConnectionHandle conn);
FLOW_FFI_EXPORT const char* flow_connection_get_start_port(FlowConnectionHandle conn);
FLOW_FFI_EXPORT const char* flow_connection_get_end_node_id(FlowConnectionHandle conn);
FLOW_FFI_EXPORT const char* flow_connection_get_end_port(FlowConnectionHandle conn);

// ============================================================================
// Node Factory Management
// ============================================================================

// Create node from factory
FLOW_FFI_EXPORT FlowNodeHandle flow_factory_create_node(FlowNodeFactoryHandle factory,
                                                        const char* class_name, const char* uuid,
                                                        const char* name, FlowEnvHandle env);

// Get available node categories
FLOW_FFI_EXPORT FlowError flow_factory_get_categories(FlowNodeFactoryHandle factory,
                                                      char*** categories, size_t* count);

// Get node classes in a category
FLOW_FFI_EXPORT FlowError flow_factory_get_node_classes(FlowNodeFactoryHandle factory,
                                                        const char* category, char*** classes,
                                                        size_t* count);

// Get friendly name for a node class
FLOW_FFI_EXPORT const char* flow_factory_get_friendly_name(FlowNodeFactoryHandle factory,
                                                           const char* class_name);

// Type conversion checking
FLOW_FFI_EXPORT bool flow_factory_is_convertible(FlowNodeFactoryHandle factory,
                                                 const char* from_type, const char* to_type);

// ============================================================================
// Module Management
// ============================================================================

// Create a module loader
FLOW_FFI_EXPORT FlowModuleHandle flow_module_create(FlowNodeFactoryHandle factory);

// Destroy a module
FLOW_FFI_EXPORT void flow_module_destroy(FlowModuleHandle module);

// Load a module
FLOW_FFI_EXPORT FlowError flow_module_load(FlowModuleHandle module, const char* path);

// Unload a module
FLOW_FFI_EXPORT FlowError flow_module_unload(FlowModuleHandle module);

// Register module nodes
FLOW_FFI_EXPORT FlowError flow_module_register_nodes(FlowModuleHandle module);

// Unregister module nodes
FLOW_FFI_EXPORT FlowError flow_module_unregister_nodes(FlowModuleHandle module);

// Check if module is loaded
FLOW_FFI_EXPORT bool flow_module_is_loaded(FlowModuleHandle module);

// Get module metadata
FLOW_FFI_EXPORT const char* flow_module_get_name(FlowModuleHandle module);
FLOW_FFI_EXPORT const char* flow_module_get_version(FlowModuleHandle module);
FLOW_FFI_EXPORT const char* flow_module_get_author(FlowModuleHandle module);
FLOW_FFI_EXPORT const char* flow_module_get_description(FlowModuleHandle module);

// ============================================================================
// Data Type Management
// ============================================================================

// Create typed data
FLOW_FFI_EXPORT FlowNodeDataHandle flow_data_create_int(int32_t value);
FLOW_FFI_EXPORT FlowNodeDataHandle flow_data_create_double(double value);
FLOW_FFI_EXPORT FlowNodeDataHandle flow_data_create_bool(bool value);
FLOW_FFI_EXPORT FlowNodeDataHandle flow_data_create_string(const char* value);

// Get typed data
FLOW_FFI_EXPORT FlowError flow_data_get_int(FlowNodeDataHandle data, int32_t* value);
// flow_data_get_int64 — for ports declared as int64_t / long / long long
// (e.g. FlowFlutterCudaPreview's "texture_id" output).  Distinct symbol from
// flow_data_get_int because int32_t* cannot hold IDs > 2^31 and we don't want
// silent truncation.
FLOW_FFI_EXPORT FlowError flow_data_get_int64(FlowNodeDataHandle data, int64_t* value);
FLOW_FFI_EXPORT FlowError flow_data_get_double(FlowNodeDataHandle data, double* value);
FLOW_FFI_EXPORT FlowError flow_data_get_bool(FlowNodeDataHandle data, bool* value);
FLOW_FFI_EXPORT FlowError flow_data_get_string(FlowNodeDataHandle data, char** value);

// Get data type
FLOW_FFI_EXPORT const char* flow_data_get_type(FlowNodeDataHandle data);

// Destroy data
FLOW_FFI_EXPORT void flow_data_destroy(FlowNodeDataHandle data);

// Convert data to string representation
FLOW_FFI_EXPORT const char* flow_data_to_string(FlowNodeDataHandle data);

// ============================================================================
// Image Data (P10 — FLOW_RUN.html Appendix B §B.4)
// ============================================================================

// Storage kind for an image payload.
typedef enum FlowImageKind {
    FLOW_IMAGE_KIND_ENCODED  = 0, // PNG/JPEG/etc. byte stream
    FLOW_IMAGE_KIND_RAW_RGBA = 1, // tightly packed or strided RGBA8, top-left origin
} FlowImageKind;

// Pixel format for RAW images.  v1 supports RGBA8888 only; enum is
// provisioned with room for BGRA/RGB/GRAY without breaking the ABI.
typedef enum FlowPixelFormat {
    FLOW_PIXEL_FORMAT_RGBA8888 = 0, // 4 bytes per pixel
} FlowPixelFormat;

// Read-only descriptor populated by flow_data_image_borrow.  The bytes
// pointer is owned by the originating FlowNodeDataHandle and remains
// valid only until flow_data_destroy is called on that handle.
typedef struct FlowImageDescriptor {
    FlowImageKind   kind;
    FlowPixelFormat format;            // ignored for ENCODED
    int32_t         width;
    int32_t         height;
    int32_t         row_stride_bytes;  // == width*4 if tightly packed (RAW only)
    const uint8_t*  bytes;             // owned by the C++ side
    size_t          bytes_length;
    uint64_t        content_version;   // monotonic; used by the Dart coalescer to dedup
} FlowImageDescriptor;

// Create an image-bearing FlowNodeData (encoded blob: PNG, JPEG, etc.).
// Bytes are copied into a refcounted shared_ptr<vector<uint8_t>> inside the
// returned handle.  width/height may be -1 ("unknown — resolve at decode").
FLOW_FFI_EXPORT FlowNodeDataHandle flow_data_create_image_encoded(
    const uint8_t* bytes, size_t length,
    int32_t width, int32_t height);

// Create an image-bearing FlowNodeData (raw RGBA8).  Bytes are copied into
// a refcounted shared_ptr<vector<uint8_t>> inside the returned handle.
// row_stride_bytes must be >= width * 4 (tightly packed when ==).
FLOW_FFI_EXPORT FlowNodeDataHandle flow_data_create_image_raw(
    const uint8_t* bytes,
    int32_t width, int32_t height,
    int32_t row_stride_bytes,
    FlowPixelFormat format);

// Borrow a read-only descriptor for an image-bearing FlowNodeData.  The
// `bytes` pointer in `out_desc` is a view into the C++ side and is valid
// until flow_data_destroy is called on the handle.  Returns
// FLOW_ERROR_TYPE_MISMATCH when the handle does not carry a flow::Image.
FLOW_FFI_EXPORT FlowError flow_data_image_borrow(
    FlowNodeDataHandle data, FlowImageDescriptor* out_desc);

// Cheap predicate — does this NodeData carry a flow::Image?  Safe to call
// on any handle; returns false for null/invalid handles.
FLOW_FFI_EXPORT bool flow_data_is_image(FlowNodeDataHandle data);

// ============================================================================
// Texture Registrar Slot (Phase D / P12 CUDA prep)
// ============================================================================

// Store a platform texture registrar pointer inside libflow_ffi.so so that
// future CUDA/GPU nodes (Phase D) can call fl_texture_registrar_register_texture
// without a dependency on flutter_linux headers.  The pointer is treated as
// opaque void* here; the Flutter plugin calls this setter at registration time.
// Thread-safe: stored with atomic store (seq_cst).
FLOW_FFI_EXPORT void flow_ffi_set_texture_registrar(void* registrar);

// Returns the most recently set registrar pointer, or NULL if never set.
// For internal use by CUDA/GPU nodes.
FLOW_FFI_EXPORT void* flow_ffi_get_texture_registrar(void);

// Returns non-zero if a registrar has been successfully set.
// Intended for Dart integration tests / diagnostics.
FLOW_FFI_EXPORT int flow_ffi_is_texture_registrar_bound(void);

// ============================================================================
// Texture Ops Callbacks (Phase D — CUDA/GL interop plugin-extension pattern)
// ============================================================================
//
// libflow_ffi.so cannot include flutter_linux headers directly (they are only
// available inside a `flutter build linux` context).  Instead, the Linux plugin
// (packages/flow_ffi_flutter/linux/) implements the FlTextureGL GObject subclass
// and exposes the three operations below as C function-pointer callbacks that it
// injects at plugin registration time via flow_ffi_set_texture_ops().
//
// All callbacks receive a `registrar` pointer (the same void* previously stored
// by flow_ffi_set_texture_registrar).  All are called on the platform/UI thread
// unless noted otherwise.
//
// create_gl_texture(registrar, width, height,
//                  out_texture_object, out_texture_id, out_gl_name)
//   Allocate a FlTextureGL subclass wrapping a pre-created GL texture of the
//   given dimensions, register it with FlTextureRegistrar, and write:
//     *out_texture_object — opaque GObject* (caller must g_object_ref if it
//                           keeps a long-lived reference)
//     *out_texture_id     — int64_t Flutter texture ID
//     *out_gl_name        — uint32_t OpenGL texture name (for CUDA interop)
//   Returns 0 on success, -1 on failure.
//
// destroy_gl_texture(registrar, texture_object, texture_id)
//   Unregister from FlTextureRegistrar, g_object_unref the texture object.
//
// mark_frame_available(registrar, texture_object)
//   Call fl_texture_registrar_mark_texture_frame_available.

typedef int  (*FlowTextureCreateFn)(void* registrar,
                                    int32_t width, int32_t height,
                                    void** out_texture_object,
                                    int64_t* out_texture_id,
                                    uint32_t* out_gl_name);

typedef void (*FlowTextureDestroyFn)(void* registrar,
                                     void* texture_object,
                                     int64_t texture_id);

typedef void (*FlowTextureMarkFrameFn)(void* registrar,
                                       void* texture_object);

// wait_initialized(texture_object, out_cuda_resource)
//   Block (up to ~1 second) until the raster thread has completed the deferred
//   GL initialisation in FlTextureGL::populate().  On success writes the
//   cudaGraphicsResource_t handle to *out_cuda_resource and returns 0.
//   Returns non-zero if initialization failed or timed out; caller must drop
//   the frame in that case.  thread-safe; may be called from any thread.
typedef int  (*FlowTextureWaitInitFn)(void* texture_object,
                                      void** out_cuda_resource);

// submit_frame(texture_object, src_device_ptr, src_pitch_bytes,
//              width, height, ready_event)
//   Copy pixels from a CUDA device buffer into the texture's staging buffer
//   (pure D→D, no GL context required).  The actual GL-coupled Map/Memcpy/
//   Unmap happens later in populate() on the raster thread.
//   ready_event: optional cudaEvent_t* (cast to void*); if non-null the
//     copy is gated on the producer event before starting.
//   Returns 0 on success, non-zero if the frame was dropped (e.g. staging
//   buffer not yet allocated, dim mismatch, or CUDA error).
//   Thread-safe; called from the cascade worker thread.
typedef int  (*FlowTextureSubmitFrameFn)(void*       texture_object,
                                         const void* src_device_ptr,
                                         int32_t     src_pitch_bytes,
                                         int32_t     width,
                                         int32_t     height,
                                         void*       ready_event);

typedef struct FlowTextureOps {
    FlowTextureCreateFn      create_gl_texture;
    FlowTextureDestroyFn     destroy_gl_texture;
    FlowTextureMarkFrameFn   mark_frame_available;
    FlowTextureWaitInitFn    wait_initialized;    // added Phase D threading fix
    FlowTextureSubmitFrameFn submit_frame;        // added Phase D data-plane fix
} FlowTextureOps;

// Called by the Linux plugin during registration to inject the callbacks.
// All five pointers must be non-null; passing NULL for ops clears the slot.
FLOW_FFI_EXPORT void  flow_ffi_set_texture_ops(const FlowTextureOps* ops);

// Returns non-zero if all five callbacks are installed.
// Used by image_bridge_cuda.cpp to gate texture operations.
FLOW_FFI_EXPORT int   flow_ffi_is_texture_ops_bound(void);

// Internal accessor — returns a copy of the stored FlowTextureOps.
// Returns all-NULL ops if not yet bound.
FLOW_FFI_EXPORT FlowTextureOps flow_ffi_get_texture_ops(void);

// ============================================================================
// GPU Texture Ops Callbacks (P12 — CPU-side host→Flutter texture path)
// ============================================================================
//
// Non-CUDA parallel to the FlowTextureOps / FlowTextureWaitInitFn block above.
// This vtable is injected by texture_sink_bridge.cc in the Linux Flutter plugin
// at registration time; libflow_ffi.so calls through it to register, destroy,
// wait, and upload textures without touching cudart at all.
//
// The submit_frame callback differs from the CUDA variant: src_bytes points to
// host (CPU) memory; row_stride_bytes is the stride of the source image (may
// be > width*4 for padded images).  The implementation must memcpy the pixel
// rows and tolerate stride padding.
//
// All operations are synchronous — no events, no streams, no async.

typedef void* FlowGpuTextureHandle;

// create_gpu_texture(registrar, width, height,
//                   out_texture_object, out_texture_id)
//   Allocate a FlTextureGL subclass, register with FlTextureRegistrar, arm the
//   bootstrap re-mark loop, and write:
//     *out_texture_object — opaque GObject* (caller holds ref)
//     *out_texture_id     — int64_t Flutter texture ID
//   Returns 0 on success, -1 on failure.
typedef int  (*FlowGpuTextureCreateFn)(void*    registrar,
                                       int32_t  width,
                                       int32_t  height,
                                       void**   out_texture_object,
                                       int64_t* out_texture_id);

// destroy_gpu_texture(registrar, texture_object, texture_id)
//   Unregister from FlTextureRegistrar and g_object_unref the texture object.
typedef void (*FlowGpuTextureDestroyFn)(void*   registrar,
                                        void*   texture_object,
                                        int64_t texture_id);

// mark_gpu_frame_available(registrar, texture_object)
//   Call fl_texture_registrar_mark_texture_frame_available.
typedef void (*FlowGpuTextureMarkFrameFn)(void* registrar,
                                          void* texture_object);

// wait_gpu_initialized(texture_object)
//   Block (up to ~1 s) until populate() has completed deferred GL init.
//   Returns 0 on success, non-zero on failure / timeout.
typedef int  (*FlowGpuTextureWaitInitFn)(void* texture_object);

// submit_gpu_frame(texture_object, src_bytes, row_stride_bytes, width, height)
//   Copy host pixels into the texture's staging buffer (pure memcpy — no GL
//   context required).  Returns 0 on success, non-zero on drop.
typedef int  (*FlowGpuTextureSubmitFrameFn)(void*       texture_object,
                                            const void* src_bytes,
                                            int32_t     row_stride_bytes,
                                            int32_t     width,
                                            int32_t     height);

typedef struct FlowGpuTextureOps {
    FlowGpuTextureCreateFn      create_texture;
    FlowGpuTextureDestroyFn     destroy_texture;
    FlowGpuTextureMarkFrameFn   mark_frame_available;
    FlowGpuTextureWaitInitFn    wait_initialized;
    FlowGpuTextureSubmitFrameFn submit_frame;
} FlowGpuTextureOps;

// Called by texture_sink_bridge.cc during plugin registration.
// All five pointers must be non-null; passing NULL clears the slot.
FLOW_FFI_EXPORT void flow_ffi_set_gpu_texture_ops(const FlowGpuTextureOps* ops);

// Returns non-zero if all five callbacks are installed.
FLOW_FFI_EXPORT int  flow_ffi_is_gpu_texture_ops_bound(void);

// Returns a copy of the stored FlowGpuTextureOps (all-NULL when not bound).
FLOW_FFI_EXPORT FlowGpuTextureOps flow_ffi_get_gpu_texture_ops(void);

// True when the GPU texture ops vtable is bound.  Always-present symbol
// (returns false on non-Linux or before the plugin registers).
FLOW_FFI_EXPORT bool flow_ffi_gpu_texture_sink_available(void);

// Register a Flutter texture of the given dimensions via the GPU texture ops.
// On success, *out and *out_texture_id are populated.
FLOW_FFI_EXPORT FlowError flow_ffi_register_flutter_texture(
    int32_t              w,
    int32_t              h,
    FlowGpuTextureHandle* out,
    int64_t*             out_texture_id);

// Unregister a previously registered Flutter texture.
FLOW_FFI_EXPORT FlowError flow_ffi_unregister_flutter_texture(
    FlowGpuTextureHandle handle);

// Synchronously copy host bytes into the texture's staging buffer, then kick
// mark_frame_available.  row_stride_bytes is the stride of the source buffer;
// may be > width*4 for padded images.
FLOW_FFI_EXPORT FlowError flow_ffi_upload_to_texture(
    FlowGpuTextureHandle handle,
    const void*          host_bytes,
    int32_t              row_stride_bytes,
    int32_t              width,
    int32_t              height);

// ============================================================================
// Memory Management Helpers
// ============================================================================

// Free strings allocated by the library
FLOW_FFI_EXPORT void flow_free_string(char* str);

// Free string arrays
FLOW_FFI_EXPORT void flow_free_string_array(char** array, size_t count);

// Free handle arrays
FLOW_FFI_EXPORT void flow_free_handle_array(void** array);

// Free connection info arrays
FLOW_FFI_EXPORT void flow_free_connection_array(FlowConnectionInfo* connections, size_t count);

// ============================================================================
// Event Callbacks (Phase 5)
// ============================================================================

// Callback function types
typedef void (*FlowNodeEventCallback)(FlowNodeHandle node, void* user_data);
typedef void (*FlowConnectionEventCallback)(FlowConnectionHandle conn, void* user_data);
typedef void (*FlowErrorEventCallback)(const char* error, void* user_data);
typedef void (*FlowNodeDataEventCallback)(FlowNodeHandle node, const char* port_key,
                                          FlowNodeDataHandle data, void* user_data);

// Event registration handle for tracking callbacks
typedef struct FlowEventRegistration* FlowEventRegistrationHandle;

// Graph Events
FLOW_FFI_EXPORT FlowEventRegistrationHandle flow_graph_on_node_added(FlowGraphHandle graph,
                                                                     FlowNodeEventCallback callback,
                                                                     void* user_data);

FLOW_FFI_EXPORT FlowEventRegistrationHandle
flow_graph_on_node_removed(FlowGraphHandle graph, FlowNodeEventCallback callback, void* user_data);

FLOW_FFI_EXPORT FlowEventRegistrationHandle flow_graph_on_nodes_connected(
    FlowGraphHandle graph, FlowConnectionEventCallback callback, void* user_data);

FLOW_FFI_EXPORT FlowEventRegistrationHandle flow_graph_on_nodes_disconnected(
    FlowGraphHandle graph, FlowConnectionEventCallback callback, void* user_data);

FLOW_FFI_EXPORT FlowEventRegistrationHandle flow_graph_on_error(FlowGraphHandle graph,
                                                                FlowErrorEventCallback callback,
                                                                void* user_data);

// Node Events
FLOW_FFI_EXPORT FlowEventRegistrationHandle flow_node_on_compute(FlowNodeHandle node,
                                                                 FlowNodeEventCallback callback,
                                                                 void* user_data);

FLOW_FFI_EXPORT FlowEventRegistrationHandle flow_node_on_error(FlowNodeHandle node,
                                                               FlowErrorEventCallback callback,
                                                               void* user_data);

FLOW_FFI_EXPORT FlowEventRegistrationHandle
flow_node_on_set_input(FlowNodeHandle node, FlowNodeDataEventCallback callback, void* user_data);

FLOW_FFI_EXPORT FlowEventRegistrationHandle
flow_node_on_set_output(FlowNodeHandle node, FlowNodeDataEventCallback callback, void* user_data);

// Event unregistration
FLOW_FFI_EXPORT FlowError flow_event_unregister(FlowEventRegistrationHandle registration);

// Event registration validation
FLOW_FFI_EXPORT bool flow_event_is_valid(FlowEventRegistrationHandle registration);

#ifdef __cplusplus
}
#endif

#endif // FLOW_FFI_H