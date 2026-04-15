#include "common.h"
#include "dijkstra.h"
#include "floodmax.h"
#include <cstring>
#include <iomanip>
#include <iostream>
#include <mpi.h>

#define MPI_CHECK(call)                                                        \
  do {                                                                         \
    int err = call;                                                            \
    if (err != MPI_SUCCESS) {                                                  \
      std::cerr << "MPI error " << err << " in " << #call << " at line "       \
                << __LINE__ << std::endl;                                      \
      MPI_Abort(MPI_COMM_WORLD, err);                                          \
    }                                                                          \
  } while (0)

void printUsage(const char *progName) {
  std::cout
      << "Usage: " << progName << " <graph_file> [options]\n"
      << "Options:\n"
      << "  -o <output_file>   Output file for results (default: results.txt)\n"
      << "  -s <source_node>   Source node ID for Dijkstra (default: "
         "auto-detect source)\n"
      << "  -i <max_iter>      Max iterations (default: 100)\n"
      << "  --help             Show this help message\n";
}

int main(int argc, char **argv) {
  MPI_CHECK(MPI_Init(&argc, &argv));

  int rank, size;
  MPI_CHECK(MPI_Comm_rank(MPI_COMM_WORLD, &rank));
  MPI_CHECK(MPI_Comm_size(MPI_COMM_WORLD, &size));

  if (argc < 2) {
    if (rank == 0)
      printUsage(argv[0]);
    MPI_CHECK(MPI_Finalize());
    return 1;
  }

  std::string graphFile = argv[1];
  std::string outputFile = "results.txt";
  int sourceNode = -1;
  int maxIterations = 100;

  for (int i = 2; i < argc; ++i) {
    if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
      outputFile = argv[++i];
    } else if (strcmp(argv[i], "-s") == 0 && i + 1 < argc) {
      sourceNode = std::stoi(argv[++i]);
    } else if (strcmp(argv[i], "-i") == 0 && i + 1 < argc) {
      maxIterations = std::stoi(argv[++i]);
    } else if (strcmp(argv[i], "--help") == 0) {
      if (rank == 0)
        printUsage(argv[0]);
      MPI_Finalize();
      return 0;
    }
  }

  Graph g;
  if (rank == 0) {
    std::cout << "Loading graph from " << graphFile << "...\n";
    g = readGraphFromJSON(graphFile);
    std::cout << "Graph: " << g.numNodes << " nodes, " << g.numEdges
              << " edges\n";

    if (sourceNode == -1) {
      sourceNode = getSourceNode(g);
      std::cout << "Auto-detected source node: " << sourceNode << "\n";
    }
  }

  MPI_CHECK(MPI_Bcast(&g.numNodes, 1, MPI_INT, 0, MPI_COMM_WORLD));
  MPI_CHECK(MPI_Bcast(&g.numEdges, 1, MPI_INT, 0, MPI_COMM_WORLD));
  MPI_CHECK(MPI_Bcast(&sourceNode, 1, MPI_INT, 0, MPI_COMM_WORLD));

  if (rank != 0) {
    g.nodes.resize(g.numNodes);
    g.edges.resize(g.numEdges);
  }

  std::vector<double> storedValues(g.numNodes);
  if (rank == 0) {
    for (int i = 0; i < g.numNodes; ++i) {
      storedValues[i] = g.nodes[i].storedValue;
    }
  }
  MPI_CHECK(MPI_Bcast(storedValues.data(), g.numNodes, MPI_DOUBLE, 0,
                      MPI_COMM_WORLD));
  if (rank != 0) {
    for (int i = 0; i < g.numNodes; ++i) {
      g.nodes[i].id = i;
      g.nodes[i].storedValue = storedValues[i];
    }
  }

  std::vector<int> adjSizes(g.numNodes);
  if (rank == 0) {
    for (int i = 0; i < g.numNodes; ++i) {
      adjSizes[i] = g.adjList[i].size();
    }
  }

  MPI_CHECK(MPI_Bcast(adjSizes.data(), g.numNodes, MPI_INT, 0, MPI_COMM_WORLD));
  int maxAdjSize = 0;
  for (int i = 0; i < g.numNodes; ++i) {
    if (adjSizes[i] > maxAdjSize)
      maxAdjSize = adjSizes[i];
  }

  std::vector<int> sendBuf(g.numNodes * (maxAdjSize + 1));
  if (rank == 0) {
    for (int i = 0; i < g.numNodes; ++i) {
      int base = i * (maxAdjSize + 1);
      sendBuf[base] = adjSizes[i];
      for (int j = 0; j < adjSizes[i]; ++j) {
        sendBuf[base + 1 + j] = g.adjList[i][j];
      }
    }
  }

  MPI_CHECK(MPI_Bcast(sendBuf.data(), g.numNodes * (maxAdjSize + 1), MPI_INT, 0,
                      MPI_COMM_WORLD));

  if (rank != 0) {
    for (int i = 0; i < g.numNodes; ++i) {
      int base = i * (maxAdjSize + 1);
      int sz = sendBuf[base];
      g.adjList[i].clear();
      for (int j = 0; j < sz; ++j) {
        g.adjList[i].push_back(sendBuf[base + 1 + j]);
      }
    }
  }

  std::vector<int> edgeFroms(g.numEdges), edgeTos(g.numEdges);
  std::vector<double> edgeCosts(g.numEdges);
  if (rank == 0) {
    for (int i = 0; i < g.numEdges; ++i) {
      edgeFroms[i] = g.edges[i].fromNodeId;
      edgeTos[i] = g.edges[i].toNodeId;
      edgeCosts[i] = g.edges[i].cost;
    }
  }
  MPI_CHECK(
      MPI_Bcast(edgeFroms.data(), g.numEdges, MPI_INT, 0, MPI_COMM_WORLD));
  MPI_CHECK(MPI_Bcast(edgeTos.data(), g.numEdges, MPI_INT, 0, MPI_COMM_WORLD));
  MPI_CHECK(
      MPI_Bcast(edgeCosts.data(), g.numEdges, MPI_DOUBLE, 0, MPI_COMM_WORLD));

  if (rank != 0) {
    g.edges.resize(g.numEdges);
    for (int i = 0; i < g.numEdges; ++i) {
      g.edges[i].fromNodeId = edgeFroms[i];
      g.edges[i].toNodeId = edgeTos[i];
      g.edges[i].cost = edgeCosts[i];
    }
  }

  std::vector<int> partitionAssignment(g.numNodes);
  std::vector<std::pair<int, int>> localNodes, borderNodes;

  if (rank == 0) {
    partitionGraph(g, size, localNodes, borderNodes);
    int start = 0;
    for (int p = 0; p < size; ++p) {
      int count = localNodes[p].second;
      for (int i = start; i < start + count; ++i) {
        partitionAssignment[i] = p;
      }
      start += count;
    }
  }

  MPI_CHECK(MPI_Bcast(partitionAssignment.data(), g.numNodes, MPI_INT, 0,
                      MPI_COMM_WORLD));

  int myStart = -1, myCount = 0;
  for (int i = 0; i < g.numNodes; ++i) {
    if (partitionAssignment[i] == rank) {
      if (myStart == -1)
        myStart = i;
      myCount++;
    }
  }
  if (myStart == -1)
    myStart = 0;

  MPI_CHECK(MPI_Barrier(MPI_COMM_WORLD));

  if (rank == 0) {
    std::cout << "\n=== Phase 1: Leader Election (FloodMax) ===\n";
  }

  double electionStart = MPI_Wtime();

  double myWeight = 0.0;
  for (int i = myStart; i < myStart + myCount; ++i) {
    if (i < g.numNodes) {
      myWeight += g.nodes[i].storedValue;
    }
  }
  myWeight += (rank + 1) * 0.001;

  FloodMax *fm = runFloodMaxElection(rank, size, &g, rank, myWeight,
                                     myStart, myCount);
  MPI_CHECK(MPI_Barrier(MPI_COMM_WORLD));

  double electionTime = MPI_Wtime() - electionStart;

  int localLeader = fm->getLeader();
  int globalLeader = -1;
  MPI_CHECK(MPI_Allreduce(&localLeader, &globalLeader, 1, MPI_INT, MPI_MAX,
                          MPI_COMM_WORLD));

  if (rank == 0) {
    std::cout << "Leader elected: rank " << globalLeader << "\n";
    std::cout << "Election completed in " << std::fixed << std::setprecision(3)
              << electionTime << "s\n";
    std::cout << "\n=== Phase 2: Distributed Dijkstra ===\n";
  }

  MPI_CHECK(MPI_Barrier(MPI_COMM_WORLD));

  double dijkstraStart = MPI_Wtime();

  DistributedDijkstra dijkstra(rank, size, &g, sourceNode, myStart, myCount);
  dijkstra.run(maxIterations);

  MPI_CHECK(MPI_Barrier(MPI_COMM_WORLD));

  double dijkstraTime = MPI_Wtime() - dijkstraStart;

  long long localMessageCount = dijkstra.getMessageCount();
  long long localBytesSent = dijkstra.getBytesSent();

  long long totalMessageCount = 0;
  long long totalBytesSent = 0;
  MPI_CHECK(MPI_Reduce(&localMessageCount, &totalMessageCount, 1, MPI_LONG_LONG,
                       MPI_SUM, 0, MPI_COMM_WORLD));
  MPI_CHECK(MPI_Reduce(&localBytesSent, &totalBytesSent, 1, MPI_LONG_LONG,
                       MPI_SUM, 0, MPI_COMM_WORLD));

  if (rank == 0) {
    std::cout << "Dijkstra completed in " << std::fixed << std::setprecision(3)
              << dijkstraTime << "s\n";
    std::cout << "Converged after " << dijkstra.getIteration()
              << " iterations\n";
    std::cout << "Total messages sent: " << totalMessageCount << "\n";
    std::cout << "Total bytes sent: " << totalBytesSent << "\n";
  }

  std::vector<DijkstraResult> localResults = dijkstra.getLocalResults();

  if (rank == 0) {
    std::ofstream out(outputFile);
    out << "NetGameSim Distributed Dijkstra Results\n";
    out << "======================================\n";
    out << "Graph: " << g.numNodes << " nodes, " << g.numEdges << " edges\n";
    out << "Source node: " << sourceNode << "\n";
    out << "Partitions: " << size << "\n";
    out << "Leader elected: rank " << globalLeader << "\n";
    out << "Election time: " << std::fixed << std::setprecision(3)
        << electionTime << "s\n";
    out << "Dijkstra time: " << std::fixed << std::setprecision(3)
        << dijkstraTime << "s\n";
    out << "Dijkstra iterations: " << dijkstra.getIteration() << "\n";
    out << "Total messages sent: " << totalMessageCount << "\n";
    out << "Total bytes sent: " << totalBytesSent << "\n\n";

    out << "Sample distances (first 20 nodes):\n";
    out << "Node ID | Distance | Predecessor | Hops\n";
    out << "--------|----------|-------------|-----\n";

    int count = 0;
    for (const auto &r : localResults) {
      if (count++ >= 20)
        break;
      out << std::setw(7) << r.nodeId << " | ";
      if (r.distance >= std::numeric_limits<double>::max() / 2) {
          out << std::setw(8) << "INF" << " | ";
      } else {
          out << std::setw(8) << std::fixed << std::setprecision(3) << r.distance << " | ";
      }
      out << std::setw(11) << r.predecessor << " | " << std::setw(4) << r.hopCount << "\n";
    }

    out.close();
    std::cout << "\nResults written to " << outputFile << "\n";
  }

  delete fm;
  MPI_CHECK(MPI_Finalize());
  return 0;
}
