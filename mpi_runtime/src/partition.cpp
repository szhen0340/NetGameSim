#include "common.h"
#include <cstring>
#include <iostream>
#include <json/json.h>
#include <mpi.h>

void printUsage(const char *progName) {
  std::cout << "Usage: " << progName << " <graph_file> [options]\n"
            << "Options:\n"
            << "  -o <output_dir>   Output directory (default: ./partitioned)\n"
            << "  -p <num_procs>   Number of partitions (default: MPI size)\n"
            << "  --help           Show this help message\n";
}

int main(int argc, char **argv) {
  MPI_Init(&argc, &argv);

  int rank, size;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);

  if (argc < 2) {
    if (rank == 0)
      printUsage(argv[0]);
    MPI_Finalize();
    return 1;
  }

  std::string graphFile = argv[1];
  std::string outputDir = "./partitioned";
  int numPartitions = size;

  for (int i = 2; i < argc; ++i) {
    if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
      outputDir = argv[++i];
    } else if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
      numPartitions = std::stoi(argv[++i]);
    } else if (strcmp(argv[i], "--help") == 0) {
      if (rank == 0)
        printUsage(argv[0]);
      MPI_Finalize();
      return 0;
    }
  }

  Graph g;
  if (rank == 0) {
    std::cout << "Reading graph from " << graphFile << "...\n";
    g = readGraphFromJSON(graphFile);
    std::cout << "Graph loaded: " << g.numNodes << " nodes, " << g.numEdges
              << " edges\n";
  }

  MPI_Bcast(&g.numNodes, 1, MPI_INT, 0, MPI_COMM_WORLD);
  MPI_Bcast(&g.numEdges, 1, MPI_INT, 0, MPI_COMM_WORLD);

  if (rank != 0) {
    g.nodes.resize(g.numNodes);
  }

  std::vector<int> nodeIds(g.numNodes);
  if (rank == 0) {
    for (int i = 0; i < g.numNodes; ++i)
      nodeIds[i] = g.nodes[i].id;
  }
  MPI_Bcast(nodeIds.data(), g.numNodes, MPI_INT, 0, MPI_COMM_WORLD);

  if (rank == 0) {
    std::cout << "Partitioning across " << numPartitions << " processes...\n";
  }

  std::vector<std::pair<int, int>> localNodes, borderNodes;
  if (rank == 0) {
    partitionGraph(g, numPartitions, localNodes, borderNodes);
  }

  std::vector<int> partitionAssignment(g.numNodes);
  if (rank == 0) {
    int start = 0;
    for (int p = 0; p < numPartitions; ++p) {
      int count = localNodes[p].second;
      for (int i = start; i < start + count; ++i) {
        partitionAssignment[i] = p;
      }
      start += count;
    }
  }

  MPI_Bcast(partitionAssignment.data(), g.numNodes, MPI_INT, 0, MPI_COMM_WORLD);

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

  if (rank == 0) {
    std::cout << "Rank 0: nodes " << myStart << " to "
              << (myStart + myCount - 1) << " (" << myCount << " nodes)\n";
  }

  MPI_Barrier(MPI_COMM_WORLD);

  if (rank == 0) {
    std::cout << "Partitioning complete.\n";
    std::cout << "Border nodes (edges crossing partitions): "
              << borderNodes.size() << "\n";
  }

  MPI_Finalize();
  return 0;
}
