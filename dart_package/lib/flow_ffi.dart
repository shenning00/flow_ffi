/// Flutter FFI bridge for the flow-core computational graph library.
///
/// This library provides a Dart interface to the C++ flow-core library,
/// enabling Flutter developers to create and execute computational graphs
/// for data processing and analysis.
library;

// Core models
export 'src/models/environment.dart';
export 'src/models/graph.dart';
export 'src/models/node.dart';
export 'src/models/connection.dart';
export 'src/models/factory.dart';
export 'src/models/module.dart';

// Utilities
export 'src/utils/error_handler.dart';
export 'src/utils/type_converter_simple.dart';
export 'src/utils/event_manager.dart';

// Services
export 'src/services/flow_service.dart';
export 'src/services/graph_builder.dart';

// Demonstrations (for users to import if needed)
export 'src/demos/basic_demo.dart';
export 'src/demos/graph_demo.dart';
export 'src/demos/event_demo.dart';
export 'src/demos/module_demo.dart';
export 'src/demos/main_demo.dart';
