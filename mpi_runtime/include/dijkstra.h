#ifndef DIJKSTRA_H
#define DIJKSTRA_H

#include "common.h"
#include <algorithm>
#include <limits>
#include <map>
#include <mpi.h>
#include <queue>
#include <set>
#include <unordered_map>
#include <vector>

struct DijkstraResult {
  int nodeId;
  double distance;
  int predecessor;
  int hopCount;

  DijkstraResult()
      : nodeId(-1), distance(std::numeric_limits<double>::max()),
        predecessor(-1), hopCount(0) {}
  DijkstraResult(int id, double dist, int pred, int hops)
      : nodeId(id), distance(dist), predecessor(pred), hopCount(hops) {}
};

struct NodeInfo {
  double distance;
  int predecessor;
  int hops;
  int ownerRank;

  NodeInfo()
      : distance(std::numeric_limits<double>::max()), predecessor(-1), hops(0),
        ownerRank(-1) {}
  NodeInfo(double dist, int pred, int h, int rank)
      : distance(dist), predecessor(pred), hops(h), ownerRank(rank) {}
};

struct PQNode {
  int nodeId;
  double distance;
  int hops;

  bool operator<(const PQNode &other) const {
    if (distance != other.distance) {
      return distance > other.distance;
    }
    if (hops != other.hops) {
      return hops < other.hops;
    }
    return nodeId < other.nodeId;
  }
};

class DistributedDijkstra {
private:
  int rank;
  int size;
  const Graph *graph;
  int sourceNode;
  int localNodeStart;
  int localNodeEnd;

  std::unordered_map<int, NodeInfo> distances;
  std::map<std::pair<int, int>, double> edgeCostMap;

  std::set<int> settledNodes;
  std::priority_queue<PQNode> localFrontier;

  bool converged;
  int iteration;

  long long messageCount;
  long long bytesSent;

  int getOwnerRank(int nodeId) const {
    for (int p = 0; p < size; ++p) {
      int pStart = (graph->numNodes / size) * p +
                   std::min(p, graph->numNodes % size);
      int pCount = (graph->numNodes / size) +
                   (p < graph->numNodes % size ? 1 : 0);
      if (nodeId >= pStart && nodeId < pStart + pCount) {
        return p;
      }
    }
    return -1;
  }

public:
  DistributedDijkstra(int r, int s, const Graph *g, int src, int start,
                       int count);
  ~DistributedDijkstra() = default;

  void initialize();
  void explorationStep();
  bool checkConvergence();
  void run(int maxIterations = 100);

  std::vector<DijkstraResult> getLocalResults() const;
  double getDistance(int nodeId) const;
  bool isConverged() const { return converged; }
  int getIteration() const { return iteration; }

  long long getMessageCount() const { return messageCount; }
  long long getBytesSent() const { return bytesSent; }
};

DistributedDijkstra::DistributedDijkstra(int r, int s, const Graph *g, int src,
                                           int start, int count)
    : rank(r), size(s), graph(g), sourceNode(src), localNodeStart(start),
      localNodeEnd(start + count), converged(false), iteration(0),
      messageCount(0), bytesSent(0) {
  for (const auto &e : graph->edges) {
    edgeCostMap[{e.fromNodeId, e.toNodeId}] = e.cost;
  }
}

void DistributedDijkstra::initialize() {
  distances.clear();
  settledNodes.clear();
  while (!localFrontier.empty())
    localFrontier.pop();

  for (int i = localNodeStart; i < localNodeEnd; ++i) {
    distances[i] = NodeInfo();
  }

  if (sourceNode >= localNodeStart && sourceNode < localNodeEnd) {
    distances[sourceNode] = NodeInfo(0.0, -1, 0, rank);
    localFrontier.push(PQNode{sourceNode, 0.0, 0});
  }
}

void DistributedDijkstra::explorationStep() {
  double localBestDist = std::numeric_limits<double>::max();
  int localBestNode = -1;
  int localBestHops = 0;

  while (!localFrontier.empty()) {
    PQNode top = localFrontier.top();
    if (settledNodes.find(top.nodeId) == settledNodes.end()) {
      localBestDist = top.distance;
      localBestNode = top.nodeId;
      localBestHops = top.hops;
      break;
    } else {
      localFrontier.pop();
    }
  }

  struct {
    double dist;
    int node;
  } local, global;

  local.dist = localBestDist;
  local.node = localBestNode;

  MPI_Allreduce(&local, &global, 1, MPI_DOUBLE_INT, MPI_MINLOC,
                MPI_COMM_WORLD);
  messageCount++;
  bytesSent += sizeof(double) + sizeof(int);

  if (global.node == -1) {
    return;
  }

  int wasISelected = (localBestNode == global.node && localBestDist == global.dist) ? 1 : 0;
  int globalSelectedRank = -1;

  if (wasISelected) {
    globalSelectedRank = rank;
  }

  int selectedRankGlobal = -1;
  MPI_Allreduce(&globalSelectedRank, &selectedRankGlobal, 1, MPI_INT, MPI_MAX,
                MPI_COMM_WORLD);
  messageCount++;
  bytesSent += sizeof(int);

  int settledNodeGlobal = global.node;
  MPI_Bcast(&settledNodeGlobal, 1, MPI_INT, selectedRankGlobal, MPI_COMM_WORLD);
  messageCount++;
  bytesSent += sizeof(int);

  double settledDist = global.dist;
  int settledHops = localBestHops;

  MPI_Bcast(&settledDist, 1, MPI_DOUBLE, selectedRankGlobal, MPI_COMM_WORLD);
  messageCount++;
  bytesSent += sizeof(double);

  MPI_Bcast(&settledHops, 1, MPI_INT, selectedRankGlobal, MPI_COMM_WORLD);
  messageCount++;
  bytesSent += sizeof(int);

  if (settledNodeGlobal >= 0) {
    settledNodes.insert(settledNodeGlobal);
    distances[settledNodeGlobal].distance = settledDist;
    distances[settledNodeGlobal].hops = settledHops;
    
    // Pop the node from the local priority queue if I am the owner
    if (rank == selectedRankGlobal) {
      localFrontier.pop();
    }
  }

  if (settledNodeGlobal >= 0) {
    auto it = graph->adjList.find(settledNodeGlobal);
    if (it != graph->adjList.end()) {
      for (int neighbor : it->second) {
        double edgeCost = 1.0;
        auto costIt = edgeCostMap.find({settledNodeGlobal, neighbor});
        if (costIt != edgeCostMap.end()) {
          edgeCost = costIt->second;
        }

        double newDist = distances[settledNodeGlobal].distance + edgeCost;
        int newHops = distances[settledNodeGlobal].hops + 1;
        int neighborOwner = getOwnerRank(neighbor);

        if (neighborOwner == rank) {
          if (distances[neighbor].distance > newDist ||
              (distances[neighbor].distance == newDist &&
               distances[neighbor].hops > newHops)) {
            distances[neighbor].distance = newDist;
            distances[neighbor].hops = newHops;
            distances[neighbor].predecessor = settledNodeGlobal;
            localFrontier.push(PQNode{neighbor, newDist, newHops});
          }
        }
      }
    }
  }

  std::vector<std::vector<int>> sendNodes(size);
  std::vector<std::vector<double>> sendDists(size);
  std::vector<std::vector<int>> sendPreds(size);
  std::vector<std::vector<int>> sendHops(size);

  std::vector<int> nodesToSend;

  for (const auto &entry : distances) {
    int node = entry.first;
    if (settledNodes.find(node) == settledNodes.end()) {
      continue;
    }
    if (node < localNodeStart || node >= localNodeEnd) {
      continue;
    }

    auto it = graph->adjList.find(node);
    if (it == graph->adjList.end()) {
      continue;
    }

    for (int neighbor : it->second) {
      int neighborOwner = getOwnerRank(neighbor);
      if (neighborOwner >= 0 && neighborOwner != rank) {
        double edgeCost = 1.0;
        auto costIt = edgeCostMap.find({node, neighbor});
        if (costIt != edgeCostMap.end()) {
          edgeCost = costIt->second;
        }
        
        sendNodes[neighborOwner].push_back(neighbor);
        sendDists[neighborOwner].push_back(entry.second.distance + edgeCost);
        sendPreds[neighborOwner].push_back(node);
        sendHops[neighborOwner].push_back(entry.second.hops + 1);
      }
    }
  }

  std::vector<int> sendCounts(size, 0);
  std::vector<int> recvCounts(size, 0);
  for (int p = 0; p < size; ++p) {
    sendCounts[p] = static_cast<int>(sendNodes[p].size());
  }

  MPI_Alltoall(sendCounts.data(), 1, MPI_INT, recvCounts.data(), 1, MPI_INT,
               MPI_COMM_WORLD);
  messageCount++;
  bytesSent += size * sizeof(int);

  std::vector<MPI_Request> reqs;

  for (int p = 0; p < size; ++p) {
    if (p == rank || sendCounts[p] == 0)
      continue;
    MPI_Request req;
    MPI_Isend(sendNodes[p].data(), sendCounts[p], MPI_INT, p, 1, MPI_COMM_WORLD,
              &req);
    reqs.push_back(req);
    MPI_Isend(sendDists[p].data(), sendCounts[p], MPI_DOUBLE, p, 2,
              MPI_COMM_WORLD, &req);
    reqs.push_back(req);
    MPI_Isend(sendPreds[p].data(), sendCounts[p], MPI_INT, p, 3, MPI_COMM_WORLD,
              &req);
    reqs.push_back(req);
    MPI_Isend(sendHops[p].data(), sendCounts[p], MPI_INT, p, 4, MPI_COMM_WORLD,
              &req);
    reqs.push_back(req);
    messageCount += 4;
    bytesSent += sendCounts[p] * (sizeof(int) * 3 + sizeof(double));
  }

  std::vector<std::vector<int>> recvNodes(size);
  std::vector<std::vector<double>> recvDists(size);
  std::vector<std::vector<int>> recvPreds(size);
  std::vector<std::vector<int>> recvHops(size);

  for (int p = 0; p < size; ++p) {
    if (p == rank || recvCounts[p] == 0)
      continue;
    recvNodes[p].resize(recvCounts[p]);
    recvDists[p].resize(recvCounts[p]);
    recvPreds[p].resize(recvCounts[p]);
    recvHops[p].resize(recvCounts[p]);
    MPI_Request req;
    MPI_Irecv(recvNodes[p].data(), recvCounts[p], MPI_INT, p, 1,
              MPI_COMM_WORLD, &req);
    reqs.push_back(req);
    MPI_Irecv(recvDists[p].data(), recvCounts[p], MPI_DOUBLE, p, 2,
              MPI_COMM_WORLD, &req);
    reqs.push_back(req);
    MPI_Irecv(recvPreds[p].data(), recvCounts[p], MPI_INT, p, 3,
              MPI_COMM_WORLD, &req);
    reqs.push_back(req);
    MPI_Irecv(recvHops[p].data(), recvCounts[p], MPI_INT, p, 4,
              MPI_COMM_WORLD, &req);
    reqs.push_back(req);
  }

  if (!reqs.empty()) {
    MPI_Waitall(reqs.size(), reqs.data(), MPI_STATUSES_IGNORE);
  }

  for (int p = 0; p < size; ++p) {
    for (int i = 0; i < recvCounts[p]; ++i) {
      int node = recvNodes[p][i];
      if (node >= localNodeStart && node < localNodeEnd &&
          settledNodes.find(node) == settledNodes.end()) {
        double newDist = recvDists[p][i];
        int newHops = recvHops[p][i];

        if (distances[node].distance > newDist ||
            (distances[node].distance == newDist &&
             distances[node].hops > newHops)) {
          distances[node].distance = newDist;
          distances[node].hops = newHops;
          distances[node].predecessor = recvPreds[p][i];
          localFrontier.push(PQNode{node, newDist, newHops});
        }
      }
    }
  }
}

bool DistributedDijkstra::checkConvergence() {
  int localSettledCount = static_cast<int>(settledNodes.size());

  if (localSettledCount >= graph->numNodes) {
    return true;
  }

  double localMinDist = std::numeric_limits<double>::max();
  if (!localFrontier.empty()) {
    localMinDist = localFrontier.top().distance;
  }

  double globalMinDist = std::numeric_limits<double>::max();
  MPI_Allreduce(&localMinDist, &globalMinDist, 1, MPI_DOUBLE, MPI_MIN,
                MPI_COMM_WORLD);
  messageCount++;
  bytesSent += sizeof(double);

  if (globalMinDist >= std::numeric_limits<double>::max() / 2) {
    return true;
  }

  return false;
}

void DistributedDijkstra::run(int maxIterations) {
  initialize();

  // On a graph with 501 nodes, we should allow at least 501 iterations
  // to settle all nodes if each iteration settles exactly one node.
  int effectiveMaxIterations = std::max(maxIterations, (int)graph->numNodes + 10);

  for (iteration = 0; iteration < effectiveMaxIterations && !converged; ++iteration) {
    explorationStep();
    converged = checkConvergence();
  }
}

std::vector<DijkstraResult> DistributedDijkstra::getLocalResults() const {
  std::vector<DijkstraResult> results;

  for (int i = localNodeStart; i < localNodeEnd; ++i) {
    auto it = distances.find(i);
    if (it != distances.end()) {
      results.emplace_back(i, it->second.distance, it->second.predecessor,
                           it->second.hops);
    } else {
      results.emplace_back(i, std::numeric_limits<double>::max(), -1, 0);
    }
  }

  return results;
}

double DistributedDijkstra::getDistance(int nodeId) const {
  auto it = distances.find(nodeId);
  if (it != distances.end()) {
    return it->second.distance;
  }
  return std::numeric_limits<double>::max();
}

#endif
