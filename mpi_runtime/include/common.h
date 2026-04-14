#ifndef COMMON_H
#define COMMON_H

#include <fstream>
#include <iostream>
#include <json/json.h>
#include <mpi.h>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

struct Node {
  int id;
  double storedValue;

  Node() : id(0), storedValue(0.0) {}
};

struct Edge {
  int fromNodeId;
  int toNodeId;
  double cost;

  Edge() : fromNodeId(0), toNodeId(0), cost(0.0) {}
};

struct Graph {
  std::vector<Node> nodes;
  std::vector<Edge> edges;
  std::unordered_map<int, std::vector<int>> adjList;

  int numNodes;
  int numEdges;

  Graph() : numNodes(0), numEdges(0) {}
};

Graph readGraphFromJSON(const std::string &filename);

void partitionGraph(const Graph &g, int numProcs,
                    std::vector<std::pair<int, int>> &localNodes,
                    std::vector<std::pair<int, int>> &borderNodes);

int getSourceNode(const Graph &g);

#endif
