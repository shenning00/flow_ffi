import 'dart:ffi';

import 'package:ffi/ffi.dart';

import '../ffi/bindings.dart';
import '../ffi/handles.dart';
import '../utils/error_handler.dart';
import 'node.dart';
import 'environment.dart';

/// Represents a factory for creating nodes and managing node types.
///
/// The NodeFactory maintains a registry of available node types organized
/// by categories, provides type conversion capabilities, and creates node
/// instances based on registered class names.
class NodeFactory {
  final FactoryHandle _handle;

  NodeFactory._(this._handle);

  /// Creates a NodeFactory wrapper for an existing handle.
  ///
  /// This is used internally and should not be called directly.
  /// Use [Environment.factory] to get a NodeFactory instance.
  NodeFactory.fromHandle(this._handle);

  /// Get the native handle for this factory.
  ///
  /// This is used internally for FFI calls.
  Pointer<FlowNodeFactory> get handle => _handle.handle;

  /// Check if this factory handle is valid.
  bool get isValid => _handle.isValid;

  /// Get the reference count for this factory.
  int get refCount => _handle.refCount;

  /// Manually retain the handle (increase reference count).
  void retain() => _handle.retain();

  /// Manually release the handle (decrease reference count).
  void release() => _handle.release();

  /// Gets all available node categories.
  ///
  /// Returns a list of category names that have registered node types.
  /// Categories are used to organize related node types together.
  ///
  /// Returns an empty list if no categories are registered.
  ///
  /// Throws [InvalidHandleException] if the factory handle is invalid.
  List<String> getCategories() {
    final categoriesPtr = calloc<Pointer<Pointer<Char>>>();
    final countPtr = calloc<Size>();

    try {
      final result = flowCore.flow_factory_get_categories(
        _handle.handle,
        categoriesPtr,
        countPtr,
      );

      ErrorHandler.checkErrorCode(result);

      final count = countPtr.value;
      if (count == 0) {
        return <String>[];
      }

      final categories = <String>[];
      final categoryArray = categoriesPtr.value;

      for (int i = 0; i < count; i++) {
        final categoryPtr = categoryArray.elementAt(i).value;
        if (categoryPtr != nullptr) {
          categories.add(categoryPtr.cast<Utf8>().toDartString());
        }
      }

      // Free the allocated memory
      flowCore.flow_free_string_array(categoryArray, count);

      return categories;
    } finally {
      calloc.free(categoriesPtr);
      calloc.free(countPtr);
    }
  }

  /// Gets all node class names in a specific category.
  ///
  /// [category] is the category name to query.
  ///
  /// Returns a list of node class names registered under the specified category.
  /// Returns an empty list if the category doesn't exist or has no classes.
  ///
  /// Throws [InvalidArgumentException] if category is null or empty.
  /// Throws [InvalidHandleException] if the factory handle is invalid.
  List<String> getNodeClasses(String category) {
    if (category.isEmpty) {
      throw const InvalidArgumentException('Category name cannot be empty');
    }

    final cCategory = category.toNativeUtf8();
    final classesPtr = calloc<Pointer<Pointer<Char>>>();
    final countPtr = calloc<Size>();

    try {
      final result = flowCore.flow_factory_get_node_classes(
        _handle.handle,
        cCategory.cast<Char>(),
        classesPtr,
        countPtr,
      );

      ErrorHandler.checkErrorCode(result);

      final count = countPtr.value;
      if (count == 0) {
        return <String>[];
      }

      final classes = <String>[];
      final classArray = classesPtr.value;

      for (int i = 0; i < count; i++) {
        final classPtr = classArray.elementAt(i).value;
        if (classPtr != nullptr) {
          classes.add(classPtr.cast<Utf8>().toDartString());
        }
      }

      // Free the allocated memory
      flowCore.flow_free_string_array(classArray, count);

      return classes;
    } finally {
      calloc.free(cCategory);
      calloc.free(classesPtr);
      calloc.free(countPtr);
    }
  }

  /// Gets the friendly name for a node class.
  ///
  /// [className] is the class name to get the friendly name for.
  ///
  /// Returns the friendly name, or the class name itself if no friendly
  /// name is registered.
  ///
  /// Throws [InvalidArgumentException] if className is null or empty.
  /// Throws [InvalidHandleException] if the factory handle is invalid.
  String getFriendlyName(String className) {
    if (className.isEmpty) {
      throw const InvalidArgumentException('Class name cannot be empty');
    }

    final cClassName = className.toNativeUtf8();
    try {
      final resultPtr = flowCore.flow_factory_get_friendly_name(
        _handle.handle,
        cClassName.cast<Char>(),
      );

      if (resultPtr == nullptr) {
        ErrorHandler.checkError();
        return className; // Fallback to class name
      }

      final result = resultPtr.cast<Utf8>().toDartString();
      flowCore.flow_free_string(resultPtr);
      return result;
    } finally {
      calloc.free(cClassName);
    }
  }

  /// Checks if one type can be converted to another.
  ///
  /// [fromType] is the source type name.
  /// [toType] is the target type name.
  ///
  /// Returns true if a conversion from [fromType] to [toType] is registered,
  /// false otherwise.
  ///
  /// Throws [InvalidArgumentException] if either type name is null or empty.
  /// Throws [InvalidHandleException] if the factory handle is invalid.
  bool isConvertible(String fromType, String toType) {
    if (fromType.isEmpty) {
      throw const InvalidArgumentException('From type cannot be empty');
    }
    if (toType.isEmpty) {
      throw const InvalidArgumentException('To type cannot be empty');
    }

    final cFromType = fromType.toNativeUtf8();
    final cToType = toType.toNativeUtf8();

    try {
      final result = flowCore.flow_factory_is_convertible(
        _handle.handle,
        cFromType.cast<Char>(),
        cToType.cast<Char>(),
      );

      // Check for errors after the call
      final errorMessage = ErrorHandler.getLastError();
      ErrorHandler.checkError(); // This will throw the appropriate exception

      return result;
    } finally {
      calloc.free(cFromType);
      calloc.free(cToType);
    }
  }

  /// Creates a new node instance.
  ///
  /// [className] is the registered class name of the node to create.
  /// [environment] is the environment the node will run in.
  /// [uuid] is an optional UUID for the node. If null, one will be generated.
  /// [name] is an optional friendly name for the node.
  ///
  /// Returns a new [Node] instance of the specified class.
  ///
  /// Throws [InvalidArgumentException] if className is null or empty.
  /// Throws [NodeNotFoundException] if the class name is not registered.
  /// Throws [InvalidHandleException] if handles are invalid.
  Node createNode(
    String className,
    Environment environment, {
    String? uuid,
    String? name,
  }) {
    if (className.isEmpty) {
      throw const InvalidArgumentException('Class name cannot be empty');
    }

    final cClassName = className.toNativeUtf8();
    final cUuid = uuid?.toNativeUtf8();
    final cName = name?.toNativeUtf8();

    try {
      final nodeHandle = flowCore.flow_factory_create_node(
        _handle.handle,
        cClassName.cast<Char>(),
        cUuid?.cast<Char>() ?? nullptr,
        cName?.cast<Char>() ?? nullptr,
        environment.handle,
      );

      if (nodeHandle == nullptr) {
        ErrorHandler.checkError();
        throw NodeNotFoundException(
            'Failed to create node of class: $className');
      }

      return Node.fromHandle(NodeHandle(nodeHandle));
    } finally {
      calloc.free(cClassName);
      if (cUuid != null) calloc.free(cUuid);
      if (cName != null) calloc.free(cName);
    }
  }

  /// Gets a map of all categories and their node classes.
  ///
  /// Returns a map where keys are category names and values are lists
  /// of node class names in that category.
  ///
  /// This is a convenience method that combines [getCategories] and
  /// [getNodeClasses] calls.
  Map<String, List<String>> getAllNodeTypes() {
    final result = <String, List<String>>{};
    final categories = getCategories();

    for (final category in categories) {
      result[category] = getNodeClasses(category);
    }

    return result;
  }

  /// Manually release the factory handle.
  ///
  /// This is typically not needed as the finalizer will handle cleanup
  /// automatically when the NodeFactory is garbage collected.
  void dispose() {
    _handle.release();
  }

  @override
  String toString() => 'NodeFactory(refCount: $refCount, isValid: $isValid)';
}
