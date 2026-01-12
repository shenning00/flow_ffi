# Flow FFI

A Flutter FFI bridge for the [flow-core](https://github.com/example/flow-core) computational graph library.

[![pub package](https://img.shields.io/pub/v/flow_ffi.svg)](https://pub.dev/packages/flow_ffi)

## Features

- **Complete FFI Bridge**: Access all flow-core functionality from Dart/Flutter
- **Type Safety**: Strongly typed Dart API with automatic memory management  
- **Event System**: Real-time graph and node events via Dart Streams
- **Module Support**: Dynamic loading of computational node modules
- **Cross Platform**: Windows, macOS, Linux, iOS, Android support

## Installation

### 1. Add Dependency

```yaml
dependencies:
  flow_ffi: ^1.0.0
```

### 2. Install Native Library

The Flow FFI package requires the native `libflow_ffi` library. 

#### Option A: Use Pre-built Binaries (Recommended)
```bash
# Download from releases page
curl -L https://github.com/example/flow_ffi/releases/latest/download/libflow_ffi-linux.tar.gz | tar xz
```

#### Option B: Build from Source
```bash
git clone https://github.com/example/flow_ffi.git
cd flow_ffi
mkdir build && cd build
cmake ..
make -j$(nproc)
```

## Quick Start

```dart
import 'package:flow_ffi/flow_ffi.dart';

void main() async {
  // Create environment with 4 threads
  final env = Environment(maxThreads: 4);
  
  // Create a computational graph
  final graph = Graph(env);
  
  // Set up event listeners
  graph.onNodeAdded.listen((event) {
    print('Node added: ${event.node.name}');
  });
  
  // Access node factory for available node types
  final factory = env.factory;
  final categories = factory.getCategories();
  print('Available node categories: $categories');
  
  // Clean up
  graph.dispose();
  factory.dispose();
  env.dispose();
}
```

## Core Concepts

### Environment
The execution environment manages thread pools and provides access to the node factory:

```dart
final env = Environment(maxThreads: 8);
final factory = env.factory;
```

### Graph
Computational graphs contain nodes and manage their connections:

```dart
final graph = Graph(env);
final node = graph.addNode('ProcessorNode', 'processor1');
graph.run(); // Execute the graph
```

### Nodes
Individual computational units with input/output ports:

```dart
// Get port information
final inputPorts = node.getInputPortKeys();
final inputType = node.getInputPortType('data');

// Set data and compute
node.setInputData('data', someData);
node.compute();
```

### Events
Real-time monitoring via Dart Streams:

```dart
// Graph events
graph.onNodeAdded.listen((event) => print('Node added'));
graph.onNodesConnected.listen((event) => print('Connection made'));

// Node events  
node.onCompute.listen((event) => print('Node computed'));
node.onSetInput.listen((event) => print('Input data changed'));
```

### Modules
Dynamic loading of node types:

```dart
final module = Module(factory);
await module.load('/path/to/module.fmod');
module.registerNodes();

// Now new node types are available
final customNode = graph.addNode('CustomNodeType', 'custom1');
```

## API Reference

### Core Classes
- **`Environment`** - Execution environment and thread management
- **`Graph`** - Computational graph container
- **`Node`** - Individual computational unit
- **`Connection`** - Link between node ports
- **`NodeFactory`** - Registry of available node types
- **`Module`** - Dynamic node type loading

### Services
- **`FlowService`** - High-level API wrapper
- **`GraphBuilder`** - Fluent API for graph construction

### Utilities
- **`ErrorHandler`** - Error management
- **`TypeConverter`** - Data type conversion
- **`EventManager`** - Event system management

## Platform Support

| Platform | Status | Notes |
|----------|--------|-------|
| Linux    | ✅ Full | Primary development platform |
| macOS    | ✅ Full | Intel and Apple Silicon |
| Windows  | ✅ Full | MSVC and MinGW |
| iOS      | ⚠️ Beta | Static linking required |
| Android  | ⚠️ Beta | NDK integration |

## Examples

See the [`example/`](example/) directory for complete working examples:

- **Simple Workflow** - Basic graph creation and execution
- **Module Loading** - Dynamic node type extension  
- **Event Handling** - Real-time graph monitoring
- **Interactive Demo** - Command-line graph builder

## Requirements

- **Dart SDK**: >=3.0.0
- **Flow-core**: C++ computational graph library
- **CMake**: >=3.16 (for building)
- **C++20**: Compatible compiler

## Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Add tests
5. Submit a pull request

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Acknowledgments

- [flow-core](https://github.com/example/flow-core) - Core computational graph library
- [FFI](https://pub.dev/packages/ffi) - Dart foreign function interface