#ifndef GRAPH_EXPORT_H
#define GRAPH_EXPORT_H

#include <random>
#include <string>
#include <unordered_map>
#include <vector>

struct ExportedNode {
  int id;
  int originalId;
  double storedValue;
  bool valuableData;
};

struct ExportedEdge {
  int fromId;
  int toId;
  double weight;
  int actionType;
};

struct ExportedGraph {
  std::vector<ExportedNode> nodes;
  std::vector<ExportedEdge> edges;
  int seed;
  int numNodes;
  int numEdges;
};

ExportedGraph loadAndEnrichGraph(const std::string &filename,
                                 int overrideSeed = -1);

void saveExportedGraph(const ExportedGraph &g, const std::string &filename);

std::string graphToJSON(const ExportedGraph &g);

#endif
