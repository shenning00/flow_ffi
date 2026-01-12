import 'dart:async';

import '../models/environment.dart';
import '../models/graph.dart';
import '../models/node.dart';
import '../models/connection.dart';
import '../models/factory.dart';
import '../utils/error_handler.dart';

/// High-level service for managing Flow operations.
///
/// This service provides a simplified API for common Flow operations,
/// handling environment setup, graph management, and event coordination.
class FlowService {
  Environment? _environment;
  Graph? _graph;
  NodeFactory? _factory;

  final StreamController<GraphEvent> _graphEventsController =
      StreamController<GraphEvent>.broadcast();
  final StreamController<NodeEvent> _nodeEventsController =
      StreamController<NodeEvent>.broadcast();
  final StreamController<FlowException> _errorsController =
      StreamController<FlowException>.broadcast();

  /// Gets the current environment, if initialized.
  Environment? get environment => _environment;

  /// Gets the current graph, if created.
  Graph? get graph => _graph;

  /// Gets the current node factory, if available.
  NodeFactory? get factory => _factory;

  /// Stream of graph-level events.
  Stream<GraphEvent> get graphEvents => _graphEventsController.stream;

  /// Stream of node-level events.
  Stream<NodeEvent> get nodeEvents => _nodeEventsController.stream;

  /// Stream of error events.
  Stream<FlowException> get errors => _errorsController.stream;

  /// Initializes the Flow service with the specified environment settings.
  ///
  /// [maxThreads] specifies the maximum number of threads for the environment.
  Future<void> initialize({int maxThreads = 4}) async {
    try {
      _environment = Environment(maxThreads: maxThreads);
      _factory = _environment!.factory;
    } catch (e) {
      final error = e is FlowException ? e : UnknownFlowException(e.toString());
      _errorsController.add(error);
      rethrow;
    }
  }

  /// Creates a new computational graph.
  ///
  /// Throws [StateError] if the service is not initialized.
  Future<Graph> createGraph() async {
    if (_environment == null) {
      throw StateError(
          'FlowService must be initialized before creating a graph');
    }

    try {
      _graph = Graph(_environment!);
      _setupGraphEventListeners(_graph!);
      return _graph!;
    } catch (e) {
      final error = e is FlowException ? e : UnknownFlowException(e.toString());
      _errorsController.add(error);
      rethrow;
    }
  }

  /// Adds a node to the current graph.
  ///
  /// [classId] is the class identifier for the node type.
  /// [name] is an optional friendly name for the node.
  ///
  /// Returns the created node instance.
  /// Throws [StateError] if no graph is available.
  Future<Node> addNode(String classId, [String? name]) async {
    if (_graph == null) {
      throw StateError('No graph available. Call createGraph() first.');
    }

    try {
      final node = _graph!.addNode(classId, name);
      _setupNodeEventListeners(node);
      return node;
    } catch (e) {
      final error = e is FlowException ? e : UnknownFlowException(e.toString());
      _errorsController.add(error);
      rethrow;
    }
  }

  /// Connects two nodes in the current graph.
  ///
  /// [sourceId] is the UUID of the source node.
  /// [sourcePort] is the name of the output port on the source node.
  /// [targetId] is the UUID of the target node.
  /// [targetPort] is the name of the input port on the target node.
  ///
  /// Returns the created connection.
  /// Throws [StateError] if no graph is available.
  Future<Connection> connectNodes(
    String sourceId,
    String sourcePort,
    String targetId,
    String targetPort,
  ) async {
    if (_graph == null) {
      throw StateError('No graph available. Call createGraph() first.');
    }

    try {
      return _graph!.connectNodes(sourceId, sourcePort, targetId, targetPort);
    } catch (e) {
      final error = e is FlowException ? e : UnknownFlowException(e.toString());
      _errorsController.add(error);
      rethrow;
    }
  }

  /// Executes the current graph.
  ///
  /// Throws [StateError] if no graph is available.
  Future<void> executeGraph() async {
    if (_graph == null) {
      throw StateError('No graph available. Call createGraph() first.');
    }

    try {
      _graph!.run();
      if (_environment != null) {
        _environment!.wait();
      }
    } catch (e) {
      final error = e is FlowException ? e : UnknownFlowException(e.toString());
      _errorsController.add(error);
      rethrow;
    }
  }

  /// Gets all available node categories from the factory.
  Future<List<String>> getCategories() async {
    if (_factory == null) {
      throw StateError('FlowService must be initialized to access factory');
    }

    try {
      return _factory!.getCategories();
    } catch (e) {
      final error = e is FlowException ? e : UnknownFlowException(e.toString());
      _errorsController.add(error);
      rethrow;
    }
  }

  /// Gets all node classes in a specific category.
  Future<List<String>> getNodeClasses(String category) async {
    if (_factory == null) {
      throw StateError('FlowService must be initialized to access factory');
    }

    try {
      return _factory!.getNodeClasses(category);
    } catch (e) {
      final error = e is FlowException ? e : UnknownFlowException(e.toString());
      _errorsController.add(error);
      rethrow;
    }
  }

  /// Sets up event listeners for the graph.
  void _setupGraphEventListeners(Graph graph) {
    graph.onNodeAdded.listen((event) {
      _graphEventsController.add(
        GraphEvent(
          'NodeAdded',
          'Node ${event.node.name} added to graph',
          event.node,
        ),
      );
    });

    graph.onNodeRemoved.listen((event) {
      _graphEventsController.add(
        GraphEvent(
          'NodeRemoved',
          'Node ${event.node.name} removed from graph',
          event.node,
        ),
      );
    });

    graph.onNodesConnected.listen((event) {
      _graphEventsController.add(
        GraphEvent(
          'NodesConnected',
          'Nodes connected: ${event.connection.description}',
          null,
          event.connection,
        ),
      );
    });

    graph.onNodesDisconnected.listen((event) {
      _graphEventsController.add(
        GraphEvent(
          'NodesDisconnected',
          'Nodes disconnected: ${event.connection.description}',
          null,
          event.connection,
        ),
      );
    });

    graph.onError.listen((event) {
      final error = UnknownFlowException(event.error);
      _errorsController.add(error);
    });
  }

  /// Sets up event listeners for a node.
  void _setupNodeEventListeners(Node node) {
    node.onCompute.listen((event) {
      _nodeEventsController.add(
        NodeEvent(
          'Compute',
          'Node ${event.node.name} computed',
          event.node,
        ),
      );
    });

    node.onError.listen((event) {
      final error = UnknownFlowException('Node error: ${event.error}');
      _errorsController.add(error);
    });

    node.onSetInput.listen((event) {
      _nodeEventsController.add(
        NodeEvent(
          'SetInput',
          'Input set on ${event.node.name}:${event.portKey}',
          event.node,
        ),
      );
    });

    node.onSetOutput.listen((event) {
      _nodeEventsController.add(
        NodeEvent(
          'SetOutput',
          'Output set on ${event.node.name}:${event.portKey}',
          event.node,
        ),
      );
    });
  }

  /// Cleanup all resources.
  Future<void> cleanup() async {
    try {
      _graph?.dispose();
      _factory?.dispose();
      _environment?.dispose();
    } catch (e) {
      final error = e is FlowException ? e : UnknownFlowException(e.toString());
      _errorsController.add(error);
    }

    await _graphEventsController.close();
    await _nodeEventsController.close();
    await _errorsController.close();
  }
}

/// Represents a graph-level event.
class GraphEvent {
  final String type;
  final String description;
  final Node? node;
  final Connection? connection;

  GraphEvent(this.type, this.description, [this.node, this.connection]);

  @override
  String toString() => 'GraphEvent($type: $description)';
}

/// Represents a node-level event.
class NodeEvent {
  final String type;
  final String description;
  final Node node;

  NodeEvent(this.type, this.description, this.node);

  @override
  String toString() => 'NodeEvent($type: $description)';
}
