#include <iostream>
#include <cassert>
#include <mpi.h>
#include "common.h"
#include "dijkstra.h"

Edge makeEdge(int from, int to, double cost) {
    Edge e;
    e.fromNodeId = from;
    e.toNodeId = to;
    e.cost = cost;
    return e;
}

bool testPartitionContiguous() {
    Graph g;
    g.numNodes = 10;
    g.numEdges = 0;
    
    for (int i = 0; i < 10; ++i) {
        Node n;
        n.id = i;
        g.nodes.push_back(n);
        g.adjList[i] = {};
    }
    
    g.adjList[0].push_back(1);
    g.adjList[0].push_back(5);
    g.adjList[1].push_back(2);
    g.adjList[5].push_back(6);
    g.adjList[6].push_back(7);
    g.numEdges = 5;
    
    g.edges.push_back(makeEdge(0, 1, 1.0));
    g.edges.push_back(makeEdge(0, 5, 1.0));
    g.edges.push_back(makeEdge(1, 2, 1.0));
    g.edges.push_back(makeEdge(5, 6, 1.0));
    g.edges.push_back(makeEdge(6, 7, 1.0));
    
    std::vector<std::pair<int, int>> localNodes;
    std::vector<std::pair<int, int>> borderNodes;
    
    partitionGraph(g, 3, localNodes, borderNodes);
    
    assert(localNodes.size() == 3);
    assert(localNodes[0].first == 0 && localNodes[0].second == 4);
    assert(localNodes[1].first == 4 && localNodes[1].second == 3);
    assert(localNodes[2].first == 7 && localNodes[2].second == 3);
    
    std::cout << "PASSED\n";
    return true;
}

bool testGetSourceNode() {
    Graph g;
    g.numNodes = 4;
    g.numEdges = 3;
    
    for (int i = 0; i < 4; ++i) {
        Node n;
        n.id = i;
        g.nodes.push_back(n);
    }
    
    g.edges.push_back(makeEdge(0, 1, 1.0));
    g.edges.push_back(makeEdge(1, 2, 1.0));
    g.edges.push_back(makeEdge(2, 3, 1.0));
    
    int source = getSourceNode(g);
    assert(source == 0);
    
    std::cout << "PASSED\n";
    return true;
}

bool testGetSourceNodeMiddle() {
    Graph g;
    g.numNodes = 4;
    g.numEdges = 4;
    
    for (int i = 0; i < 4; ++i) {
        Node n;
        n.id = i;
        g.nodes.push_back(n);
    }
    
    g.edges.push_back(makeEdge(0, 2, 1.0));
    g.edges.push_back(makeEdge(1, 2, 1.0));
    g.edges.push_back(makeEdge(2, 3, 1.0));
    g.edges.push_back(makeEdge(2, 1, 1.0));
    
    int source = getSourceNode(g);
    assert(source == 0 || source == 1);
    
    std::cout << "PASSED\n";
    return true;
}

bool testGraphConnectivity() {
    Graph g;
    g.numNodes = 5;
    
    for (int i = 0; i < 5; ++i) {
        Node n;
        n.id = i;
        g.nodes.push_back(n);
        g.adjList[i] = {};
    }
    
    g.adjList[0].push_back(1);
    g.adjList[1].push_back(2);
    g.adjList[2].push_back(3);
    g.adjList[3].push_back(4);
    g.adjList[4].push_back(0);
    
    g.edges.push_back(makeEdge(0, 1, 1.0));
    g.edges.push_back(makeEdge(1, 2, 1.0));
    g.edges.push_back(makeEdge(2, 3, 1.0));
    g.edges.push_back(makeEdge(3, 4, 1.0));
    g.edges.push_back(makeEdge(4, 0, 1.0));
    g.numEdges = 5;
    
    int source = getSourceNode(g);
    assert(source == 0);
    
    std::cout << "PASSED\n";
    return true;
}

bool testPartitionBorderNodes() {
    Graph g;
    g.numNodes = 6;
    
    for (int i = 0; i < 6; ++i) {
        Node n;
        n.id = i;
        g.nodes.push_back(n);
        g.adjList[i] = {};
    }
    
    g.adjList[0].push_back(1);
    g.adjList[1].push_back(2);
    g.adjList[1].push_back(3);
    g.adjList[3].push_back(4);
    g.adjList[4].push_back(5);
    
    g.edges.push_back(makeEdge(0, 1, 1.0));
    g.edges.push_back(makeEdge(1, 2, 1.0));
    g.edges.push_back(makeEdge(1, 3, 2.5));
    g.edges.push_back(makeEdge(3, 4, 1.0));
    g.edges.push_back(makeEdge(4, 5, 1.0));
    g.numEdges = 5;
    
    std::vector<std::pair<int, int>> localNodes;
    std::vector<std::pair<int, int>> borderNodes;
    
    partitionGraph(g, 2, localNodes, borderNodes);
    
    assert(localNodes.size() == 2);
    assert(localNodes[0].first == 0 && localNodes[0].second == 3);
    assert(localNodes[1].first == 3 && localNodes[1].second == 3);
    
    bool foundBorderNode = false;
    for (const auto& bn : borderNodes) {
        if (bn.first == 1 && bn.second == 3) {
            foundBorderNode = true;
            break;
        }
    }
    assert(foundBorderNode);
    
    std::cout << "PASSED\n";
    return true;
}

bool testGetSourceNodeAllIncoming() {
    Graph g;
    g.numNodes = 3;
    g.numEdges = 3;
    
    for (int i = 0; i < 3; ++i) {
        Node n;
        n.id = i;
        g.nodes.push_back(n);
    }
    
    g.edges.push_back(makeEdge(0, 1, 1.0));
    g.edges.push_back(makeEdge(1, 2, 1.0));
    g.edges.push_back(makeEdge(2, 0, 1.0));
    
    int source = getSourceNode(g);
    assert(source == 0);
    
    std::cout << "PASSED\n";
    return true;
}

bool testDijkstraCorrectness() {
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    
    Graph g;
    g.numNodes = 4;
    g.numEdges = 4;
    
    for (int i = 0; i < 4; ++i) {
        Node n;
        n.id = i;
        g.nodes.push_back(n);
        g.adjList[i] = {};
    }
    
    g.adjList[0].push_back(1);
    g.adjList[1].push_back(2);
    g.adjList[0].push_back(2);
    g.adjList[2].push_back(3);
    
    g.edges.push_back(makeEdge(0, 1, 1.0));
    g.edges.push_back(makeEdge(1, 2, 2.0));
    g.edges.push_back(makeEdge(0, 2, 4.0));
    g.edges.push_back(makeEdge(2, 3, 1.0));
    
    std::vector<std::pair<int, int>> localNodes;
    std::vector<std::pair<int, int>> borderNodes;
    partitionGraph(g, size, localNodes, borderNodes);
    
    int myStart = -1, myCount = 0;
    int start = 0;
    for (int p = 0; p < size; ++p) {
        if (p == rank) {
            myStart = start;
            myCount = localNodes[p].second;
        }
        start += localNodes[p].second;
    }
    
    DistributedDijkstra dijkstra(rank, size, &g, 0, myStart, myCount);
    dijkstra.run(10);
    
    if (size == 1) {
        auto results = dijkstra.getLocalResults();
        assert(results.size() == 4);
        assert(results[0].distance == 0.0);
        assert(results[1].distance == 1.0);
        assert(results[2].distance == 3.0);
        assert(results[3].distance == 4.0);
    }
    
    std::cout << "PASSED\n";
    return true;
}

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    
    if (rank == 0) {
        std::cout << "=== MPI Runtime Unit Tests ===\n";
    }
    
    int passed = 0;
    int failed = 0;
    
    if (rank == 0) std::cout << "Test: Partition contiguous... ";
    if (testPartitionContiguous()) passed++; else failed++;
    
    if (rank == 0) std::cout << "Test: Get source node (source at 0)... ";
    if (testGetSourceNode()) passed++; else failed++;
    
    if (rank == 0) std::cout << "Test: Get source node (source in middle)... ";
    if (testGetSourceNodeMiddle()) passed++; else failed++;
    
    if (rank == 0) std::cout << "Test: Graph connectivity... ";
    if (testGraphConnectivity()) passed++; else failed++;
    
    if (rank == 0) std::cout << "Test: Partition border nodes... ";
    if (testPartitionBorderNodes()) passed++; else failed++;
    
    if (rank == 0) std::cout << "Test: Get source node (all have incoming)... ";
    if (testGetSourceNodeAllIncoming()) passed++; else failed++;

    if (rank == 0) std::cout << "Test: Dijkstra shortest paths correctness... ";
    if (testDijkstraCorrectness()) passed++; else failed++;
    
    if (rank == 0) {
        std::cout << "\n=== Results: " << passed << " passed, " << failed << " failed ===\n";
    }
    
    MPI_Finalize();
    return failed > 0 ? 1 : 0;
}
