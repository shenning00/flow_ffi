import 'dart:ffi';
import 'dart:convert';

import 'package:ffi/ffi.dart';

import '../ffi/handles.dart';
import '../ffi/bindings.dart';
import '../utils/error_handler.dart';
import 'factory.dart';

/// Metadata information for a flow module.
class ModuleMetaData {
  final String name;
  final String version;
  final String author;
  final String description;

  const ModuleMetaData({
    required this.name,
    required this.version,
    required this.author,
    required this.description,
  });

  @override
  String toString() =>
      'ModuleMetaData(name: $name, version: $version, author: $author)';
}

/// Exception thrown when module operations fail.
class ModuleException implements Exception {
  final String message;
  final int? errorCode;

  const ModuleException(this.message, [this.errorCode]);

  @override
  String toString() =>
      'ModuleException: $message${errorCode != null ? ' (code: $errorCode)' : ''}';
}

/// Represents a dynamically loadable flow module.
///
/// Modules enable plugin-based extensibility of the flow system, allowing
/// runtime addition of new node types without recompilation.
///
/// A module contains:
/// - Node classes that can be registered with a NodeFactory
/// - Metadata describing the module (name, version, author, description)
/// - Platform-specific shared library (.dll/.so/.dylib)
///
/// ## Example Usage
///
/// ```dart
/// final env = Environment(maxThreads: 4);
/// final factory = env.factory;
/// final module = Module(factory);
///
/// // Load module from directory
/// await module.load('/path/to/my_module');
///
/// if (module.isLoaded) {
///   // Register nodes with factory
///   module.registerNodes();
///
///   // Access metadata
///   final metadata = module.metadata;
///   print('Loaded: ${metadata?.name} v${metadata?.version}');
///
///   // Nodes can now be created using the factory
///   final node = graph.addNode('MyModuleNode', 'instance1');
/// }
/// ```
class Module {
  final ModuleHandle _handle;
  static final FlowCoreBindings _bindings = FlowCoreBindings.instance;

  Module._(this._handle);

  /// Creates a Module wrapper for an existing handle.
  ///
  /// This is used internally and should not be called directly.
  Module.fromHandle(this._handle);

  /// Creates a new Module associated with the given factory.
  ///
  /// The module will use this factory for registering/unregistering node types.
  ///
  /// Throws [ModuleException] if module creation fails.
  factory Module(NodeFactory factory) {
    final handle = _bindings.native.flow_module_create(factory.handle);
    if (handle == nullptr) {
      final error = ErrorHandler.getLastError();
      throw ModuleException(
          'Failed to create module: ${error.message}', error.code.value);
    }

    return Module._(ModuleHandle(handle));
  }

  /// Get the native handle for this module.
  ///
  /// This is used internally for FFI calls.
  Pointer<FlowModule> get handle => _handle.handle;

  /// Check if this module handle is valid.
  bool get isValid => _handle.isValid;

  /// Get the reference count for this module.
  int get refCount => _handle.refCount;

  /// Manually retain the handle (increase reference count).
  void retain() => _handle.retain();

  /// Manually release the handle (decrease reference count).
  void release() => _handle.release();

  /// Check if the module is currently loaded.
  ///
  /// A loaded module has successfully loaded its shared library and
  /// is ready for node registration.
  bool get isLoaded {
    if (!isValid) return false;
    return _bindings.native.flow_module_is_loaded(_handle.handle);
  }

  /// Get the module metadata.
  ///
  /// Returns null if the module is not loaded or has no metadata.
  /// The metadata contains information about the module such as name,
  /// version, author, and description.
  ModuleMetaData? get metadata {
    if (!isValid || !isLoaded) return null;

    final name = _bindings.native.flow_module_get_name(_handle.handle);
    final version = _bindings.native.flow_module_get_version(_handle.handle);
    final author = _bindings.native.flow_module_get_author(_handle.handle);
    final description =
        _bindings.native.flow_module_get_description(_handle.handle);

    if (name == nullptr ||
        version == nullptr ||
        author == nullptr ||
        description == nullptr) {
      return null;
    }

    return ModuleMetaData(
      name: name.cast<Utf8>().toDartString(),
      version: version.cast<Utf8>().toDartString(),
      author: author.cast<Utf8>().toDartString(),
      description: description.cast<Utf8>().toDartString(),
    );
  }

  /// Load a module from the specified directory path.
  ///
  /// The directory should contain:
  /// - A module metadata file (*.fmod.json)
  /// - A platform-specific shared library
  ///
  /// Returns true if the module was loaded successfully.
  ///
  /// Throws [ModuleException] if loading fails or if an invalid path is provided.
  ///
  /// ## Example
  /// ```dart
  /// final success = await module.load('/path/to/module');
  /// if (success) {
  ///   print('Module loaded successfully');
  /// }
  /// ```
  Future<bool> load(String path) async {
    if (!isValid) {
      throw const ModuleException('Module handle is invalid');
    }

    if (path.isEmpty) {
      throw const ModuleException('Module path cannot be empty');
    }

    final pathPtr = path.toNativeUtf8();
    try {
      final result = _bindings.native
          .flow_module_load(_handle.handle, pathPtr.cast<Char>());

      if (result != 0) {
        // FLOW_SUCCESS = 0
        final error = ErrorHandler.getLastError();
        throw ModuleException(
            'Failed to load module: ${error.message}', error.code.value);
      }

      return true;
    } finally {
      malloc.free(pathPtr);
    }
  }

  /// Unload the currently loaded module.
  ///
  /// This will unload the shared library and clear any registered nodes.
  /// After unloading, [isLoaded] will return false.
  ///
  /// Returns true if the module was unloaded successfully.
  ///
  /// Throws [ModuleException] if unloading fails.
  ///
  /// ## Example
  /// ```dart
  /// if (module.isLoaded) {
  ///   module.unregisterNodes(); // Unregister nodes first
  ///   final success = await module.unload();
  ///   if (success) {
  ///     print('Module unloaded successfully');
  ///   }
  /// }
  /// ```
  Future<bool> unload() async {
    if (!isValid) {
      throw const ModuleException('Module handle is invalid');
    }

    if (!isLoaded) {
      return true; // Already unloaded
    }

    final result = _bindings.native.flow_module_unload(_handle.handle);

    if (result != 0) {
      // FLOW_SUCCESS = 0
      final error = ErrorHandler.getLastError();
      throw ModuleException(
          'Failed to unload module: ${error.message}', error.code.value);
    }

    return true;
  }

  /// Register this module's node types with the associated factory.
  ///
  /// This makes the module's node classes available for instantiation
  /// through the NodeFactory. The module must be loaded before calling this.
  ///
  /// Throws [ModuleException] if registration fails or the module is not loaded.
  ///
  /// ## Example
  /// ```dart
  /// await module.load('/path/to/module');
  /// module.registerNodes(); // Node types now available
  ///
  /// // Can now create nodes from this module
  /// final node = graph.addNode('ModuleNodeType', 'instance1');
  /// ```
  void registerNodes() {
    if (!isValid) {
      throw const ModuleException('Module handle is invalid');
    }

    if (!isLoaded) {
      throw const ModuleException(
          'Module must be loaded before registering nodes');
    }

    final result = _bindings.native.flow_module_register_nodes(_handle.handle);

    if (result != 0) {
      // FLOW_SUCCESS = 0
      final error = ErrorHandler.getLastError();
      throw ModuleException('Failed to register module nodes: ${error.message}',
          error.code.value);
    }
  }

  /// Unregister this module's node types from the associated factory.
  ///
  /// This removes the module's node classes from the factory registry.
  /// Existing node instances will continue to work, but new instances
  /// cannot be created until the module is registered again.
  ///
  /// Throws [ModuleException] if unregistration fails or the module is not loaded.
  ///
  /// ## Example
  /// ```dart
  /// module.unregisterNodes(); // Node types no longer available
  /// // factory.createNode('ModuleNodeType') would now fail
  /// ```
  void unregisterNodes() {
    if (!isValid) {
      throw const ModuleException('Module handle is invalid');
    }

    if (!isLoaded) {
      throw const ModuleException(
          'Module must be loaded before unregistering nodes');
    }

    final result =
        _bindings.native.flow_module_unregister_nodes(_handle.handle);

    if (result != 0) {
      // FLOW_SUCCESS = 0
      final error = ErrorHandler.getLastError();
      throw ModuleException(
          'Failed to unregister module nodes: ${error.message}',
          error.code.value);
    }
  }

  /// Manually release the module handle.
  ///
  /// This decreases the reference count and may destroy the module
  /// if this was the last reference. After calling dispose(), this
  /// Module instance should not be used.
  ///
  /// This is typically not needed as the finalizer will handle cleanup
  /// automatically when the Module is garbage collected.
  ///
  /// ## Example
  /// ```dart
  /// module.unregisterNodes();
  /// await module.unload();
  /// module.dispose(); // Clean up resources
  /// ```
  void dispose() {
    _handle.release();
  }

  @override
  String toString() {
    final meta = metadata;
    if (meta != null) {
      return 'Module(${meta.name} v${meta.version}, loaded: $isLoaded, refCount: $refCount)';
    }
    return 'Module(loaded: $isLoaded, refCount: $refCount, isValid: $isValid)';
  }
}
