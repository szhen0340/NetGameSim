#include "graph_export.h"
#include <cassert>
#include <iostream>

static std::string TEST_GRAPH_PATH = "";

bool testLoadEnrichGraph() {
  std::cout << "Test: loadAndEnrichGraph... ";
  ExportedGraph g = loadAndEnrichGraph(TEST_GRAPH_PATH, 42);
  assert(g.numNodes > 0);
  assert(g.numEdges > 0);
  assert(g.seed == 42);
  std::cout << "PASSED\n";
  return true;
}

bool testSequentialNodeIds() {
  std::cout << "Test: Sequential node IDs... ";
  ExportedGraph g = loadAndEnrichGraph(TEST_GRAPH_PATH, 100);
  for (int i = 0; i < g.numNodes; ++i) {
    assert(g.nodes[i].id == i);
  }
  std::cout << "PASSED\n";
  return true;
}

bool testPositiveWeights() {
  std::cout << "Test: Positive edge weights... ";
  ExportedGraph g = loadAndEnrichGraph(TEST_GRAPH_PATH, 100);
  for (const auto &e : g.edges) {
    assert(e.weight > 0.0);
    assert(e.weight <= 20.0);
  }
  std::cout << "PASSED\n";
  return true;
}

bool testSeedReproducibility() {
  std::cout << "Test: Seed reproducibility... ";
  ExportedGraph g1 = loadAndEnrichGraph(TEST_GRAPH_PATH, 12345);
  ExportedGraph g2 = loadAndEnrichGraph(TEST_GRAPH_PATH, 12345);
  assert(g1.seed == g2.seed);
  assert(g1.numNodes == g2.numNodes);
  assert(g1.numEdges == g2.numEdges);
  for (size_t i = 0; i < g1.edges.size(); ++i) {
    assert(g1.edges[i].weight == g2.edges[i].weight);
  }
  std::cout << "PASSED\n";
  return true;
}

bool testGraphToJSON() {
  std::cout << "Test: Graph to JSON... ";
  ExportedGraph g = loadAndEnrichGraph(TEST_GRAPH_PATH, 42);
  std::string json = graphToJSON(g);
  assert(json.find("\"seed\": 42") != std::string::npos);
  assert(json.find("\"numNodes\"") != std::string::npos);
  assert(json.find("\"numEdges\"") != std::string::npos);
  std::cout << "PASSED\n";
  return true;
}

bool testSaveLoadRoundTrip() {
  std::cout << "Test: Save/Load round trip... ";
  ExportedGraph g1 = loadAndEnrichGraph(TEST_GRAPH_PATH, 999);
  saveExportedGraph(g1, "/tmp/test_roundtrip.json");

  ExportedGraph g2 = loadAndEnrichGraph("/tmp/test_roundtrip.json", -1);
  assert(g1.seed == g2.seed);
  assert(g1.numNodes == g2.numNodes);
  assert(g1.numEdges == g2.numEdges);
  std::cout << "PASSED\n";
  return true;
}

int main(int argc, char **argv) {
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <path-to-ngs-file>\n";
    return 1;
  }
  TEST_GRAPH_PATH = argv[1];
  std::cout << "=== Graph Export Unit Tests ===\n";
  std::cout << "Using graph: " << TEST_GRAPH_PATH << "\n";

  int passed = 0;
  int failed = 0;

  try {
    if (testLoadEnrichGraph())
      passed++;
    else
      failed++;
    if (testSequentialNodeIds())
      passed++;
    else
      failed++;
    if (testPositiveWeights())
      passed++;
    else
      failed++;
    if (testSeedReproducibility())
      passed++;
    else
      failed++;
    if (testGraphToJSON())
      passed++;
    else
      failed++;
    if (testSaveLoadRoundTrip())
      passed++;
    else
      failed++;
  } catch (const std::exception &e) {
    std::cerr << "Exception: " << e.what() << "\n";
    failed++;
  }

  std::cout << "\n=== Results: " << passed << " passed, " << failed
            << " failed ===\n";
  return failed > 0 ? 1 : 0;
}
