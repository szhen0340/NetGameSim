#include "graph_export.h"
#include <cstring>
#include <iostream>
#include <fstream>

void printUsage(const char *progName) {
  std::cout << "Usage: " << progName << " <input_ngs_file> [options]\n"
            << "Options:\n"
            << "  -o <output_file>   Output file (default: stdout as JSON)\n"
            << "  -s <seed>          Override random seed for reproducibility\n"
            << "  --help             Show this help message\n";
}

int main(int argc, char **argv) {
  if (argc < 2) {
    printUsage(argv[0]);
    return 1;
  }

  std::string inputFile = argv[1];
  std::string outputFile = "";
  int overrideSeed = -1;

  for (int i = 2; i < argc; ++i) {
    if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
      outputFile = argv[++i];
    } else if (strcmp(argv[i], "-s") == 0 && i + 1 < argc) {
      overrideSeed = std::stoi(argv[++i]);
    } else if (strcmp(argv[i], "--help") == 0) {
      printUsage(argv[0]);
      return 0;
    }
  }

  ExportedGraph g = loadAndEnrichGraph(inputFile, overrideSeed);

  if (g.numNodes == 0) {
    std::cerr << "Error: Failed to load graph" << std::endl;
    return 1;
  }

  std::cout << "Graph enriched successfully:\n";
  std::cout << "  Nodes: " << g.numNodes << "\n";
  std::cout << "  Edges: " << g.numEdges << "\n";
  std::cout << "  Seed: " << g.seed << "\n";

  if (outputFile.empty()) {
    std::cout << "\n" << graphToJSON(g) << std::endl;
  } else {
    saveExportedGraph(g, outputFile);
    std::cout << "\nGraph saved to: " << outputFile << std::endl;
  }

  return 0;
}
