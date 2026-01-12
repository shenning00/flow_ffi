# Flow FFI - Flutter Bridge for Flow-Core

A comprehensive Flutter Foreign Function Interface (FFI) bridge for the flow-core computational graph library.

## Project Status

**Phase 1: Foundation - COMPLETED ✅**
**Phase 2: Environment and Factory - COMPLETED ✅**
**Phase 3: Node Model Bridge - COMPLETED ✅**
**Phase 4: Graph and Connections - COMPLETED ✅**
**Phase 5: Event System - COMPLETED ✅**
**Phase 6: Module System - COMPLETED ✅**

### Completed Features

**Phase 1:**
- ✅ CMake build system for shared library
- ✅ Complete C API header with handle type definitions  
- ✅ Thread-safe handle management system with reference counting
- ✅ Comprehensive error handling infrastructure with thread-local storage
- ✅ Dart package structure with FFI bindings
- ✅ Handle wrapper classes with automatic memory management
- ✅ Type conversion utilities
- ✅ Unit test suite for C++ components
- ✅ Dart test suite for FFI integration

**Phase 2:**
- ✅ Environment creation with configurable thread pools
- ✅ NodeFactory access and management
- ✅ Node type registration queries and category listing
- ✅ Type conversion compatibility checking
- ✅ Basic node creation capabilities (for registered classes)
- ✅ Environment variable access functionality
- ✅ Thread pool task management (Wait functionality)
- ✅ Complete C++ bridge implementations for Env and NodeFactory
- ✅ Comprehensive Dart model classes (Environment, NodeFactory)
- ✅ Full error handling with typed exceptions
- ✅ Test coverage for all Phase 2 functionality

**Phase 3:**
- ✅ Full node property access from Dart
- ✅ Working data flow through ports
- ✅ Type-safe data conversion
- ✅ Node computation triggering
- ✅ JSON serialization support
- ✅ Complete node_bridge.cpp implementation with all operations
- ✅ Full type_conversions.cpp with support for int, double, bool, string
- ✅ Updated Dart Node model with comprehensive functionality
- ✅ Extensive test suites for both C++ and Dart components
- ✅ Data handle lifecycle management with reference counting
- ✅ Thread-safe data operations and error handling

**Phase 4:**
- ✅ Complete graph management from Dart
- ✅ Node connection capabilities
- ✅ Graph execution triggering
- ✅ Full graph persistence support
- ✅ Complete C++ bridge implementations for Graph and Connection
- ✅ Full Dart model classes with comprehensive API coverage
- ✅ Connection query and management functionality
- ✅ JSON serialization/deserialization for graph state

**Phase 5:**
- ✅ Complete event callback system between C++ and Dart
- ✅ Thread-safe event registration and cleanup
- ✅ Dart Stream-based event API for all graph and node events
- ✅ Graph events: OnNodeAdded, OnNodeRemoved, OnNodesConnected, OnNodesDisconnected, OnError
- ✅ Node events: OnCompute, OnError, OnSetInput, OnSetOutput
- ✅ Event registration handle management with automatic cleanup
- ✅ Native callback trampolines with proper memory management
- ✅ Event data type system for type-safe event handling

**Phase 6:**
- ✅ Dynamic module loading from .fmod directories
- ✅ Module metadata access (name, version, author, description)
- ✅ Runtime node type registration and unregistration
- ✅ Platform-specific shared library loading (Windows/Linux/macOS)
- ✅ Complete module lifecycle management (load/unload/register/unregister)
- ✅ Thread-safe module operations with comprehensive error handling
- ✅ Full C++ bridge implementation (module_bridge.cpp)
- ✅ Complete Dart Module model with async loading support
- ✅ Module exception hierarchy with detailed error information
- ✅ Unit test coverage for both C++ and Dart components

### Architecture

```
Flutter App (Dart) 
       ↓
Handle Wrappers (Dart)
       ↓  
FFI Bindings (Dart)
       ↓
C API Layer (flow_ffi.h)
       ↓
Handle Manager (C++)
       ↓
Flow-Core Library (C++)
```

## Building

### Prerequisites

- CMake 3.16+
- C++20 compatible compiler
- Flow-core library (built in ../flow-core)
- Dart SDK 3.0+

### Build C++ Library

```bash
mkdir build
cd build
cmake ..
make
```

### Build Dart Package

```bash
cd dart_package
dart pub get
dart run ffigen
dart test
```

## Usage

**Phases 1-6**: Complete FFI bridge with event system and dynamic module loading is now available.

```dart
import 'package:flow_ffi/flow_ffi.dart';

void main() async {
  // Phases 1-5: Complete flow-core functionality with events
  try {
    // Create environment with 4 threads
    final env = Environment(maxThreads: 4);
    print('Environment created successfully');
    
    // Create a graph for computational workflow
    final graph = Graph(env);
    print('Graph created');
    
    // Set up event listeners for graph events
    graph.onNodeAdded.listen((event) {
      print('Node added: ${event.node.name}');
    });
    
    graph.onNodesConnected.listen((event) {
      print('Nodes connected: ${event.connection.startNodeId} -> ${event.connection.endNodeId}');
    });
    
    graph.onError.listen((event) {
      print('Graph error: ${event.error}');
    });
    
    // Get the node factory
    final factory = env.factory;
    print('Factory obtained');
    
    // Query available categories (will be empty until nodes are registered)
    final categories = factory.getCategories();
    print('Available categories: $categories');
    
    // Example of node creation and event handling
    try {
      final node = graph.addNode('ProcessorNode', 'processor1');
      
      // Set up node event listeners
      node.onCompute.listen((event) {
        print('Node ${event.node.name} computed');
      });
      
      node.onSetInput.listen((event) {
        print('Input set on ${event.node.name}, port: ${event.portKey}');
      });
      
      // Trigger node computation
      node.compute();
      
    } catch (e) {
      print('Node operations require registered node types: $e');
    }
    
    // Run the graph (will execute any connected nodes)
    graph.run();
    
    // Wait for any background tasks
    env.wait();
    
    // Clean up (events are automatically unregistered)
    graph.dispose();
    factory.dispose();
    env.dispose();
    
  } catch (FlowException e) {
    print('Flow error: $e');
  }
}
```

### Module Loading Example (Phase 6)

```dart
import 'package:flow_ffi/flow_ffi.dart';

void main() async {
  try {
    // Create environment and factory
    final env = Environment(maxThreads: 4);
    final factory = env.factory;
    final graph = Graph(env);
    
    // Create a module for dynamic node loading
    final module = Module(factory);
    print('Module created');
    
    // Load module from directory (contains .fmod.json and shared library)
    await module.load('/path/to/my_custom_module');
    
    if (module.isLoaded) {
      // Access module metadata
      final metadata = module.metadata;
      if (metadata != null) {
        print('Loaded module: ${metadata.name} v${metadata.version}');
        print('Author: ${metadata.author}');
        print('Description: ${metadata.description}');
      }
      
      // Register the module's node types with the factory
      module.registerNodes();
      print('Module nodes registered');
      
      // Now you can create nodes from the module
      try {
        final customNode = graph.addNode('MyCustomNodeType', 'custom1');
        print('Created custom node: ${customNode.name}');
        
        // Use the custom node in your graph
        customNode.compute();
      } catch (e) {
        print('Custom node creation failed: $e');
      }
    }
    
    // Clean up when done
    try {
      module.unregisterNodes();
      await module.unload();
      module.dispose();
      print('Module cleaned up successfully');
    } catch (ModuleException e) {
      print('Module cleanup error: $e');
    }
    
    // Clean up environment
    graph.dispose();
    factory.dispose();
    env.dispose();
    
  } catch (Exception e) {
    print('Error: $e');
  }
}
```

## Testing

### C++ Tests
```bash
cd build
make test
# Or run directly:
./flow_ffi_tests
```

### Dart Tests  
```bash
cd dart_package
dart test
```

## Next Steps - Phase 6+

- Module system for dynamic node loading and registration
- Advanced parallel task execution capabilities  
- Custom type conversion registration
- Platform integration and optimization

See [timeline.md](timeline.md) for complete implementation roadmap.

## Event System Features

The Phase 5 event system provides comprehensive event handling:

### Graph Events
- **OnNodeAdded**: Fired when nodes are added to the graph
- **OnNodeRemoved**: Fired when nodes are removed from the graph
- **OnNodesConnected**: Fired when nodes are connected
- **OnNodesDisconnected**: Fired when nodes are disconnected
- **OnError**: Fired when graph-level errors occur

### Node Events
- **OnCompute**: Fired when a node's compute method is called
- **OnError**: Fired when node-level errors occur
- **OnSetInput**: Fired when input data is set on a node port
- **OnSetOutput**: Fired when output data is set on a node port

### Thread Safety
- All events are thread-safe and properly marshaled to the Dart main isolate
- Event registrations are automatically cleaned up when objects are disposed
- Native callback trampolines prevent crashes and memory leaks

## License

[Add appropriate license]