#include "common.h"

Graph readGraphFromJSON(const std::string &filename) {
  Graph g;
  Json::Value root;
  Json::CharReaderBuilder builder;
  std::string errs;
  std::ifstream file(filename);
  if (!file.is_open()) {
    std::cerr << "Error: Cannot open file " << filename << std::endl;
    return g;
  }
  if (!Json::parseFromStream(builder, file, &root, &errs)) {
    std::cerr << "Error parsing JSON: " << errs << std::endl;
    return g;
  }
  file.close();

  Json::Value nodesJson = root["nodes"];
  Json::Value edgesJson = root["edges"];

  g.nodes.resize(nodesJson.size());
  for (const auto &n : nodesJson) {
    Node node;
    node.id = n["id"].asInt();
    node.storedValue = n["storedValue"].asDouble();
    g.nodes[node.id] = node;
  }
  g.numNodes = g.nodes.size();

  for (const auto &e : edgesJson) {
    Edge edge;
    edge.fromNodeId = e["fromId"].asInt();
    edge.toNodeId = e["toId"].asInt();
    edge.cost = e["weight"].asDouble();
    g.edges.push_back(edge);
    g.adjList[edge.fromNodeId].push_back(edge.toNodeId);
  }
  g.numEdges = g.edges.size();

  return g;
}

void partitionGraph(const Graph &g, int numProcs,
                    std::vector<std::pair<int, int>> &localNodes,
                    std::vector<std::pair<int, int>> &borderNodes) {
  int nodesPerProc = g.numNodes / numProcs;
  int remainder = g.numNodes % numProcs;

  localNodes.clear();
  borderNodes.clear();

  int start = 0;
  for (int p = 0; p < numProcs; ++p) {
    int count = nodesPerProc + (p < remainder ? 1 : 0);
    localNodes.emplace_back(start, count);

    int end = start + count - 1;
    for (int i = start; i <= end; ++i) {
      auto it = g.adjList.find(i);
      if (it != g.adjList.end()) {
        for (int neighbor : it->second) {
          if (neighbor < start || neighbor > end) {
            borderNodes.emplace_back(i, neighbor);
            break;
          }
        }
      }
    }
    start += count;
  }
}

int getSourceNode(const Graph &g) {
  std::unordered_set<int> hasIncoming;
  for (const auto &e : g.edges) {
    hasIncoming.insert(e.toNodeId);
  }
  for (int i = 0; i < g.numNodes; ++i) {
    if (hasIncoming.find(i) == hasIncoming.end()) {
      return i;
    }
  }
  return 0;
}
