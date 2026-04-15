#ifndef FLOODMAX_H
#define FLOODMAX_H

#include "common.h"
#include <iostream>
#include <mpi.h>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <vector>

struct FloodMaxMessage {
  int round;
  int candidateId;
  double candidateWeight;

  FloodMaxMessage() : round(0), candidateId(-1), candidateWeight(0.0) {}
  FloodMaxMessage(int r, int id, double w)
      : round(r), candidateId(id), candidateWeight(w) {}
};

class FloodMax {
private:
  int rank;
  int size;
  int myNodeId;
  double myNodeWeight;
  int currentRound;
  bool leaderElected;
  int leaderId;
  double leaderWeight;

  int localNodeStart;
  int localNodeEnd;
  const Graph *graph;

  int bestCandidateId;
  double bestCandidateWeight;

  std::unordered_map<int, std::vector<int>> adjList;
  std::unordered_map<int, std::unordered_set<int>> neighborRanks;

  std::unordered_map<int, int> candidateVotes;
  int noChangeCount;

public:
  FloodMax(int r, int s, const Graph *g, int id, double weight, int start,
           int count);
  ~FloodMax() = default;

  void buildAdjacencyMap();
  void sendCandidateToNeighbors();
  void receiveAndUpdateCandidates();
  bool hasConverged();
  bool runElection(int maxRounds = 100);

  int getLeader() const { return leaderId; }
  double getLeaderWeight() const { return leaderWeight; }
  bool isLeaderElected() const { return leaderElected; }
  int getRound() const { return currentRound; }
  long long getMessageCount() const { return messageCount; }
  long long getBytesSent() const { return bytesSent; }

  long long messageCount;
  long long bytesSent;
};

FloodMax::FloodMax(int r, int s, const Graph *g, int id, double weight,
                    int start, int count)
    : rank(r), size(s), myNodeId(id), myNodeWeight(weight),
      currentRound(0), leaderElected(false), leaderId(-1), leaderWeight(0.0),
      localNodeStart(start), localNodeEnd(start + count), graph(g),
      bestCandidateId(id), bestCandidateWeight(weight), noChangeCount(0),
      messageCount(0), bytesSent(0) {}

void FloodMax::buildAdjacencyMap() {
  for (const auto &entry : graph->adjList) {
    int nodeId = entry.first;
    if (nodeId < localNodeStart || nodeId >= localNodeEnd)
      continue;

    adjList[nodeId] = entry.second;

    for (int neighbor : entry.second) {
      int neighborRank = -1;
      for (int p = 0; p < size; ++p) {
        int pStart = (graph->numNodes / size) * p +
                     std::min(p, graph->numNodes % size);
        int pCount = (graph->numNodes / size) +
                     (p < graph->numNodes % size ? 1 : 0);
        if (neighbor >= pStart && neighbor < pStart + pCount) {
          neighborRank = p;
          break;
        }
      }
      if (neighborRank != -1 && neighborRank != rank) {
        neighborRanks[nodeId].insert(neighborRank);
      }
    }
  }
}

void FloodMax::sendCandidateToNeighbors() {
  std::vector<std::vector<FloodMaxMessage>> sendBuffers(size);
  std::vector<std::vector<FloodMaxMessage>> recvBuffers(size);

  for (const auto &entry : adjList) {
    int nodeId = entry.first;

    for (int neighborRank : neighborRanks[nodeId]) {
      sendBuffers[neighborRank].push_back(
          FloodMaxMessage(currentRound, bestCandidateId, bestCandidateWeight));
    }
  }

  std::vector<int> sendCounts(size, 0);
  for (int p = 0; p < size; ++p) {
    sendCounts[p] = sendBuffers[p].size();
  }

  std::vector<int> recvCounts(size, 0);
  MPI_Alltoall(sendCounts.data(), 1, MPI_INT, recvCounts.data(), 1, MPI_INT,
               MPI_COMM_WORLD);
  messageCount += 2;
  bytesSent += size * sizeof(int);

  std::vector<MPI_Request> reqs;

  for (int p = 0; p < size; ++p) {
    if (p == rank || sendCounts[p] == 0)
      continue;
    MPI_Request req;
    MPI_Isend(sendBuffers[p].data(), sendCounts[p] * sizeof(FloodMaxMessage),
              MPI_BYTE, p, 1, MPI_COMM_WORLD, &req);
    reqs.push_back(req);
    messageCount++;
    bytesSent += sendCounts[p] * sizeof(FloodMaxMessage);
  }

  for (int p = 0; p < size; ++p) {
    if (p == rank || recvCounts[p] == 0)
      continue;
    recvBuffers[p].resize(recvCounts[p]);
    MPI_Request req;
    MPI_Irecv(recvBuffers[p].data(), recvCounts[p] * sizeof(FloodMaxMessage),
              MPI_BYTE, p, 1, MPI_COMM_WORLD, &req);
    reqs.push_back(req);
  }

  if (!reqs.empty()) {
    MPI_Waitall(reqs.size(), reqs.data(), MPI_STATUSES_IGNORE);
  }

  for (int p = 0; p < size; ++p) {
    for (const auto &msg : recvBuffers[p]) {
      if (msg.candidateWeight > bestCandidateWeight ||
          (msg.candidateWeight == bestCandidateWeight &&
           msg.candidateId > bestCandidateId)) {
        bestCandidateId = msg.candidateId;
        bestCandidateWeight = msg.candidateWeight;
      }
    }
  }
}

bool FloodMax::hasConverged() {
  struct {
    double weight;
    int id;
  } local, global;

  local.weight = bestCandidateWeight;
  local.id = bestCandidateId;

  MPI_Allreduce(&local, &global, 1, MPI_DOUBLE_INT, MPI_MAXLOC,
                MPI_COMM_WORLD);
  messageCount++;
  bytesSent += sizeof(double) + sizeof(int);

  int localConverged = (global.weight == bestCandidateWeight && global.id == bestCandidateId) ? 1 : 0;
  int allConverged = 0;
  MPI_Allreduce(&localConverged, &allConverged, 1, MPI_INT, MPI_LAND,
                MPI_COMM_WORLD);
  messageCount++;
  bytesSent += sizeof(int);

  return allConverged == 1;
}

bool FloodMax::runElection(int maxRounds) {
  buildAdjacencyMap();

  for (currentRound = 1; currentRound <= maxRounds; ++currentRound) {
    MPI_Barrier(MPI_COMM_WORLD);

    sendCandidateToNeighbors();

    MPI_Barrier(MPI_COMM_WORLD);

    if (hasConverged()) {
      leaderElected = true;
      leaderId = bestCandidateId;
      leaderWeight = bestCandidateWeight;
      return true;
    }
  }

  leaderElected = true;
  leaderId = bestCandidateId;
  leaderWeight = bestCandidateWeight;
  return true;
}

FloodMax *runFloodMaxElection(int rank, int size, const Graph *graph,
                               int myNodeId, double myNodeWeight,
                               int localStart, int localCount) {
  FloodMax *fm =
      new FloodMax(rank, size, graph, myNodeId, myNodeWeight, localStart,
                   localCount);
  fm->runElection(size * 2);
  return fm;
}

#endif