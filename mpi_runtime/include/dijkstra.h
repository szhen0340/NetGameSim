#ifndef DIJKSTRA_H
#define DIJKSTRA_H

#include "common.h"
#include <algorithm>
#include <limits>
#include <map>
#include <mpi.h>
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

  NodeInfo()
      : distance(std::numeric_limits<double>::max()), predecessor(-1), hops(0) {
  }
  NodeInfo(double dist, int pred, int h)
      : distance(dist), predecessor(pred), hops(h) {}
};

class DistributedDijkstra {
private:
  int rank;
  int size;
  const Graph *graph;
  int sourceNode;
  int localNodeStart;
  int localNodeEnd;

  std::map<int, NodeInfo> distances;
  std::map<std::pair<int, int>, double> edgeCostMap;
  bool converged;
  int iteration;

  long long messageCount;
  long long bytesSent;
  double prevGlobalSum;

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
      messageCount(0), bytesSent(0), prevGlobalSum(-1.0) {
  for (const auto &e : graph->edges) {
    edgeCostMap[{e.fromNodeId, e.toNodeId}] = e.cost;
  }
}

void DistributedDijkstra::initialize() {
  distances.clear();
  if (sourceNode >= localNodeStart && sourceNode < localNodeEnd) {
    distances[sourceNode] = NodeInfo(0.0, -1, 0);
  }
}

void DistributedDijkstra::explorationStep() {
  std::map<int, NodeInfo> updates;

  for (const auto &entry : distances) {
    int node = entry.first;
    double currentDist = entry.second.distance;
    int hops = entry.second.hops;

    auto it = graph->adjList.find(node);
    if (it == graph->adjList.end())
      continue;

    for (int neighbor : it->second) {
      double edgeCost = 1.0;
      auto costIt = edgeCostMap.find({node, neighbor});
      if (costIt != edgeCostMap.end()) {
        edgeCost = costIt->second;
      }

      double newDist = currentDist + edgeCost;
      int newHops = hops + 1;

      auto existing = distances.find(neighbor);
      bool shouldUpdate = false;

      if (existing == distances.end()) {
        shouldUpdate = true;
      } else if (newDist < existing->second.distance ||
                 (newDist == existing->second.distance &&
                  newHops < existing->second.hops)) {
        shouldUpdate = true;
      }

      if (shouldUpdate) {
        updates[neighbor] = NodeInfo(newDist, node, newHops);
      }
    }
  }

  for (const auto &update : updates) {
    int node = update.first;
    NodeInfo info = update.second;

    auto existing = distances.find(node);
    if (existing == distances.end() ||
        info.distance < existing->second.distance ||
        (info.distance == existing->second.distance &&
         info.hops < existing->second.hops)) {
      distances[node] = info;
    }
  }

  std::vector<std::vector<int>> sendNodes(size);
  std::vector<std::vector<double>> sendDists(size);
  std::vector<std::vector<int>> sendPreds(size);
  std::vector<std::vector<int>> sendHops(size);

  for (const auto &update : updates) {
    int neighbor = update.first;

    for (int p = 0; p < size; ++p) {
      if (p == rank)
        continue;

      int start =
          (graph->numNodes / size) * p + std::min(p, graph->numNodes % size);
      int count =
          (graph->numNodes / size) + (p < graph->numNodes % size ? 1 : 0);

      if (neighbor >= start && neighbor < start + count) {
        sendNodes[p].push_back(update.first);
        sendDists[p].push_back(update.second.distance);
        sendPreds[p].push_back(update.second.predecessor);
        sendHops[p].push_back(update.second.hops);
        break;
      }
    }
  }

  std::vector<int> sendCounts(size, 0);
  std::vector<int> recvCounts(size, 0);
  for (int p = 0; p < size; ++p) {
    sendCounts[p] = sendNodes[p].size();
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

    MPI_Status status;
    MPI_Recv(recvNodes[p].data(), recvCounts[p], MPI_INT, p, 1, MPI_COMM_WORLD,
             &status);
    MPI_Recv(recvDists[p].data(), recvCounts[p], MPI_DOUBLE, p, 2,
             MPI_COMM_WORLD, &status);
    MPI_Recv(recvPreds[p].data(), recvCounts[p], MPI_INT, p, 3, MPI_COMM_WORLD,
             &status);
    MPI_Recv(recvHops[p].data(), recvCounts[p], MPI_INT, p, 4, MPI_COMM_WORLD,
             &status);

    for (int i = 0; i < recvCounts[p]; ++i) {
      int node = recvNodes[p][i];
      if (node >= localNodeStart && node < localNodeEnd) {
        auto existing = distances.find(node);
        if (existing == distances.end() ||
            recvDists[p][i] < existing->second.distance ||
            (recvDists[p][i] == existing->second.distance &&
             recvHops[p][i] < existing->second.hops)) {
          distances[node] =
              NodeInfo(recvDists[p][i], recvPreds[p][i], recvHops[p][i]);
        }
      }
    }
  }

  if (!reqs.empty()) {
    MPI_Waitall(reqs.size(), reqs.data(), MPI_STATUSES_IGNORE);
  }
}

bool DistributedDijkstra::checkConvergence() {
  double localMinUnfinalized = std::numeric_limits<double>::max();
  for (const auto &entry : distances) {
    if (entry.second.distance < localMinUnfinalized) {
      localMinUnfinalized = entry.second.distance;
    }
  }

  double globalMin = 0.0;
  MPI_Allreduce(&localMinUnfinalized, &globalMin, 1, MPI_DOUBLE, MPI_MIN,
                MPI_COMM_WORLD);
  messageCount++;
  bytesSent += size * sizeof(double);

  double localSum = 0.0;
  for (const auto &entry : distances) {
    if (entry.second.distance < std::numeric_limits<double>::max()) {
      localSum += entry.second.distance;
    }
  }

  double globalSum = 0.0;
  MPI_Allreduce(&localSum, &globalSum, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);

  if (iteration > 0 && globalSum == prevGlobalSum) {
    prevGlobalSum = -1.0;
    return true;
  }
  prevGlobalSum = globalSum;
  return false;
}

void DistributedDijkstra::run(int maxIterations) {
  initialize();

  for (iteration = 0; iteration < maxIterations && !converged; ++iteration) {
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
