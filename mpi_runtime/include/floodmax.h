#ifndef FLOODMAX_H
#define FLOODMAX_H

#include "common.h"
#include <iostream>
#include <mpi.h>
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
  int size;
  int leaderId;
  double leaderWeight;
  int currentRound;
  bool leaderElected;
  std::vector<int> children;
  int parent;

  int bestCandidateId;
  double bestCandidateWeight;

public:
  FloodMax(int r, int s, int id, double weight);
  ~FloodMax() = default;

  void addChild(int childRank);
  void setParent(int p);
  bool electionStep(std::vector<FloodMaxMessage> &outgoing);
  void receiveMessage(const FloodMaxMessage &msg);
  int getLeader() const { return leaderId; }
  double getLeaderWeight() const { return leaderWeight; }
  bool isLeaderElected() const { return leaderElected; }
  int getRound() const { return currentRound; }
  int getParent() const { return parent; }
  const std::vector<int> &getChildren() const { return children; }
};

FloodMax::FloodMax(int r, int s, int id, double weight)
    : size(s), leaderId(-1), leaderWeight(0.0), currentRound(0),
      leaderElected(false), parent(-1), bestCandidateId(id),
      bestCandidateWeight(weight) {}

void FloodMax::addChild(int childRank) { children.push_back(childRank); }

void FloodMax::setParent(int p) { parent = p; }

void FloodMax::receiveMessage(const FloodMaxMessage &msg) {
  if (msg.candidateWeight > bestCandidateWeight ||
      (msg.candidateWeight == bestCandidateWeight &&
       msg.candidateId > bestCandidateId)) {
    bestCandidateId = msg.candidateId;
    bestCandidateWeight = msg.candidateWeight;
  }
}

bool FloodMax::electionStep(std::vector<FloodMaxMessage> &outgoing) {
  outgoing.clear();

  if (leaderElected) {
    return true;
  }

  ++currentRound;

  outgoing.emplace_back(currentRound, bestCandidateId, bestCandidateWeight);

  int maxRounds = size * 2;
  if (currentRound >= maxRounds) {
    leaderElected = true;
    leaderId = bestCandidateId;
    leaderWeight = bestCandidateWeight;
  }

  return leaderElected;
}

FloodMax *runFloodMaxElection(int rank, int size, int myNodeId,
                              double myNodeWeight,
                              const std::vector<int> &children, int parent) {
  FloodMax *fm = new FloodMax(rank, size, myNodeId, myNodeWeight);

  for (int child : children) {
    fm->addChild(child);
  }
  if (parent >= 0) {
    fm->setParent(parent);
  }

  struct {
    double weight;
    int id;
  } local, global;

  local.weight = myNodeWeight;
  local.id = myNodeId;

  MPI_Allreduce(&local, &global, 1, MPI_DOUBLE_INT, MPI_MAXLOC, MPI_COMM_WORLD);

  std::vector<FloodMaxMessage> outgoing;
  int iterations = 0;
  const int maxIterations = size * 2;

  while (!fm->isLeaderElected() && iterations < maxIterations) {
    fm->electionStep(outgoing);

    FloodMaxMessage bestMsg(iterations + 1, global.id, global.weight);
    fm->receiveMessage(bestMsg);

    ++iterations;

    if (fm->isLeaderElected()) {
      break;
    }

    MPI_Barrier(MPI_COMM_WORLD);
  }

  return fm;
}

#endif
