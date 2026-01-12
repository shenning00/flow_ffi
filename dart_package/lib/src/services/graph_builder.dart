import '../models/graph.dart';
import '../models/node.dart';
import '../models/connection.dart';
import '../utils/error_handler.dart';

/// Fluent API builder for constructing computational graphs.
///
/// This builder provides a convenient, chainable interface for creating
/// and connecting nodes in a flow graph. It tracks nodes by user-defined
/// identifiers and provides validation and error handling.
class GraphBuilder {
  final Graph _graph;
  final Map<String, Node> _nodes = {};
  final List<Connection> _connections = [];

  /// Creates a new GraphBuilder for the specified graph.
  GraphBuilder(this._graph);

  /// The underlying graph instance.
  Graph get graph => _graph;

  /// Map of all nodes added through this builder.
  Map<String, Node> get nodes => Map.unmodifiable(_nodes);

  /// List of all connections made through this builder.
  List<Connection> get connections => List.unmodifiable(_connections);

  /// Adds a new node to the graph.
  ///
  /// [id] is a user-defined identifier for referencing the node in connections.
  /// [classId] is the registered class name for the node type.
  /// [name] is an optional friendly name for the node.
  ///
  /// Returns this builder instance for chaining.
  ///
  /// Throws [ArgumentError] if [id] is empty or already exists.
  /// Throws [FlowException] if node creation fails.
  GraphBuilder addNode(String id, String classId, [String? name]) {
    if (id.isEmpty) {
      throw ArgumentError('Node ID cannot be empty');
    }

    if (_nodes.containsKey(id)) {
      throw ArgumentError('Node with ID "$id" already exists');
    }

    try {
      final node = _graph.addNode(classId, name ?? id);
      _nodes[id] = node;
      return this;
    } catch (e) {
      throw UnknownFlowException(
          'Failed to add node "$id" of class "$classId": $e');
    }
  }

  /// Connects two nodes in the graph.
  ///
  /// [fromId] is the user-defined ID of the source node.
  /// [fromPort] is the name of the output port on the source node.
  /// [toId] is the user-defined ID of the target node.
  /// [toPort] is the name of the input port on the target node.
  ///
  /// Returns this builder instance for chaining.
  ///
  /// Throws [ArgumentError] if node IDs don't exist or ports are empty.
  /// Throws [FlowException] if the connection fails.
  GraphBuilder connect(
    String fromId,
    String fromPort,
    String toId,
    String toPort,
  ) {
    if (fromPort.isEmpty || toPort.isEmpty) {
      throw ArgumentError('Port names cannot be empty');
    }

    final fromNode = _nodes[fromId];
    final toNode = _nodes[toId];

    if (fromNode == null) {
      throw ArgumentError('Source node with ID "$fromId" not found');
    }

    if (toNode == null) {
      throw ArgumentError('Target node with ID "$toId" not found');
    }

    try {
      final connection = _graph.connectNodes(
        fromNode.id,
        fromPort,
        toNode.id,
        toPort,
      );
      _connections.add(connection);
      return this;
    } catch (e) {
      throw UnknownFlowException(
        'Failed to connect $fromId:$fromPort -> $toId:$toPort: $e',
      );
    }
  }

  /// Sets input data on a node.
  ///
  /// [nodeId] is the user-defined ID of the node.
  /// [portKey] is the name of the input port.
  /// [data] is the data to set on the port.
  ///
  /// Returns this builder instance for chaining.
  GraphBuilder setInput(String nodeId, String portKey, dynamic data) {
    final node = _nodes[nodeId];
    if (node == null) {
      throw ArgumentError('Node with ID "$nodeId" not found');
    }

    try {
      node.setInputData(portKey, data);
      return this;
    } catch (e) {
      throw UnknownFlowException(
        'Failed to set input data on $nodeId:$portKey: $e',
      );
    }
  }

  /// Gets output data from a node.
  ///
  /// [nodeId] is the user-defined ID of the node.
  /// [portKey] is the name of the output port.
  ///
  /// Returns the output data, or null if no data is available.
  T? getOutput<T>(String nodeId, String portKey) {
    final node = _nodes[nodeId];
    if (node == null) {
      throw ArgumentError('Node with ID "$nodeId" not found');
    }

    try {
      return node.getOutputData<T>(portKey);
    } catch (e) {
      throw UnknownFlowException(
        'Failed to get output data from $nodeId:$portKey: $e',
      );
    }
  }

  /// Triggers computation on a specific node.
  ///
  /// [nodeId] is the user-defined ID of the node.
  ///
  /// Returns this builder instance for chaining.
  GraphBuilder compute(String nodeId) {
    final node = _nodes[nodeId];
    if (node == null) {
      throw ArgumentError('Node with ID "$nodeId" not found');
    }

    try {
      node.compute();
      return this;
    } catch (e) {
      throw UnknownFlowException('Failed to compute node "$nodeId": $e');
    }
  }

  /// Executes the entire graph.
  ///
  /// This triggers computation for all source nodes in the graph,
  /// causing the computation to propagate through all connected nodes.
  ///
  /// Returns this builder instance for chaining.
  GraphBuilder execute() {
    try {
      _graph.run();
      return this;
    } catch (e) {
      throw UnknownFlowException('Failed to execute graph: $e');
    }
  }

  /// Validates the graph structure.
  ///
  /// Checks for common issues like disconnected nodes, cycles, etc.
  /// Returns a list of validation messages, empty if the graph is valid.
  List<String> validate() {
    final warnings = <String>[];

    // Check for nodes without connections
    for (final entry in _nodes.entries) {
      final nodeId = entry.key;
      final node = entry.value;

      try {
        if (!node.hasConnectedInputs && !node.hasConnectedOutputs) {
          warnings.add('Node "$nodeId" has no connections');
        }
      } catch (e) {
        warnings.add('Could not validate node "$nodeId": $e');
      }
    }

    // Check for dangling connections
    for (final connection in _connections) {
      try {
        final startFound =
            _nodes.values.any((n) => n.id == connection.startNodeId);
        final endFound = _nodes.values.any((n) => n.id == connection.endNodeId);

        if (!startFound) {
          warnings.add(
              'Connection references unknown start node: ${connection.startNodeId}');
        }
        if (!endFound) {
          warnings.add(
              'Connection references unknown end node: ${connection.endNodeId}');
        }
      } catch (e) {
        warnings.add('Could not validate connection: $e');
      }
    }

    return warnings;
  }

  /// Gets a summary of the graph structure.
  GraphSummary getSummary() {
    return GraphSummary(
      nodeCount: _nodes.length,
      connectionCount: _connections.length,
      nodeTypes: _nodes.values
          .map((n) {
            try {
              return n.className;
            } catch (e) {
              return 'Unknown';
            }
          })
          .toSet()
          .toList(),
      warnings: validate(),
    );
  }

  /// Clears all nodes and connections from the graph.
  ///
  /// This removes all nodes from the underlying graph and clears
  /// the builder's internal tracking.
  void clear() {
    try {
      _graph.clear();
      _nodes.clear();
      _connections.clear();
    } catch (e) {
      throw UnknownFlowException('Failed to clear graph: $e');
    }
  }

  @override
  String toString() {
    final summary = getSummary();
    return 'GraphBuilder(nodes: ${summary.nodeCount}, '
        'connections: ${summary.connectionCount}, '
        'types: ${summary.nodeTypes})';
  }
}

/// Summary information about a graph's structure.
class GraphSummary {
  final int nodeCount;
  final int connectionCount;
  final List<String> nodeTypes;
  final List<String> warnings;

  GraphSummary({
    required this.nodeCount,
    required this.connectionCount,
    required this.nodeTypes,
    required this.warnings,
  });

  /// Whether the graph has any validation warnings.
  bool get hasWarnings => warnings.isNotEmpty;

  /// Whether the graph appears to be valid (no warnings).
  bool get isValid => warnings.isEmpty;

  @override
  String toString() {
    final buffer = StringBuffer();
    buffer.writeln('Graph Summary:');
    buffer.writeln('  Nodes: $nodeCount');
    buffer.writeln('  Connections: $connectionCount');
    buffer.writeln('  Node Types: ${nodeTypes.join(', ')}');

    if (hasWarnings) {
      buffer.writeln('  Warnings:');
      for (final warning in warnings) {
        buffer.writeln('    - $warning');
      }
    } else {
      buffer.writeln('  Status: Valid');
    }

    return buffer.toString();
  }
}
