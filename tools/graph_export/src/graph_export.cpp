#include "graph_export.h"
#include <iostream>
#include <fstream>
#include <json/json.h>
#include <random>

ExportedGraph loadAndEnrichGraph(const std::string &filename,
                                 int overrideSeed) {
  ExportedGraph g;
  std::ifstream file(filename);
  if (!file.is_open()) {
    std::cerr << "Error: Cannot open file " << filename << std::endl;
    return g;
  }

  std::stringstream buffer;
  buffer << file.rdbuf();
  std::string content = buffer.str();
  file.close();

  std::vector<std::string> lines;
  std::string line;
  std::istringstream stream(content);
  while (std::getline(stream, line)) {
    lines.push_back(line);
  }

  bool isNGSFormat =
      (lines.size() >= 2 && lines[0].find("[") != std::string::npos);

  Json::Value rootJson;
  Json::Value nodesJson, edgesJson;
  Json::CharReaderBuilder builder;
  std::string errs;

  if (isNGSFormat) {
    std::istringstream nodeStream(lines[0]);
    std::istringstream edgeStream(lines[1]);
    Json::parseFromStream(builder, nodeStream, &nodesJson, &errs);
    Json::parseFromStream(builder, edgeStream, &edgesJson, &errs);
  } else {
    Json::CharReaderBuilder jsonBuilder;
    std::istringstream jsonStream(content);
    Json::parseFromStream(jsonBuilder, jsonStream, &rootJson, &errs);
    if (rootJson["nodes"].isArray() && rootJson["edges"].isArray()) {
      nodesJson = rootJson["nodes"];
      edgesJson = rootJson["edges"];
    } else if (rootJson.isArray()) {
      edgesJson = rootJson;
      nodesJson = Json::arrayValue;
    }
  }

  int storedSeed = -1;
  if (!isNGSFormat && rootJson.isObject() && rootJson["seed"].isNumeric()) {
    storedSeed = rootJson["seed"].asInt();
  }

  std::unordered_map<int, int> idMapping;
  g.nodes.reserve(nodesJson.size());

  for (const auto &n : nodesJson) {
    int originalId = n["id"].asInt();
    ExportedNode node;
    node.id = g.nodes.size();
    node.originalId = originalId;
    node.storedValue = n["storedValue"].asDouble();
    node.valuableData = n["valuableData"].asBool();
    idMapping[originalId] = node.id;
    g.nodes.push_back(node);
  }
  g.numNodes = g.nodes.size();

  std::mt19937 rng;
  if (overrideSeed >= 0) {
    g.seed = overrideSeed;
    rng.seed(static_cast<unsigned int>(g.seed));
  } else if (storedSeed >= 0) {
    g.seed = storedSeed;
    rng.seed(static_cast<unsigned int>(g.seed));
  } else {
    std::random_device rd;
    g.seed = static_cast<int>(rd());
    rng.seed(static_cast<unsigned int>(g.seed));
  }

  std::uniform_real_distribution<double> weightDist(1.0, 20.0);

  g.edges.reserve(edgesJson.size());
  for (const auto &e : edgesJson) {
    int fromOriginal = e["fromNode"]["id"].asInt();
    int toOriginal = e["toNode"]["id"].asInt();

    if (idMapping.find(fromOriginal) == idMapping.end() ||
        idMapping.find(toOriginal) == idMapping.end()) {
      continue;
    }

    ExportedEdge edge;
    edge.fromId = idMapping[fromOriginal];
    edge.toId = idMapping[toOriginal];
    edge.weight = weightDist(rng);
    edge.actionType = e["actionType"].asInt();
    g.edges.push_back(edge);
  }
  g.numEdges = g.edges.size();

  return g;
}

void saveExportedGraph(const ExportedGraph &g, const std::string &filename) {
  std::ofstream out(filename);
  if (!out.is_open()) {
    std::cerr << "Error: Cannot open file for writing: " << filename
              << std::endl;
    return;
  }

  out << "{\n";
  out << "  \"seed\": " << g.seed << ",\n";
  out << "  \"numNodes\": " << g.numNodes << ",\n";
  out << "  \"numEdges\": " << g.numEdges << ",\n";
  out << "  \"nodes\": [";
  for (size_t i = 0; i < g.nodes.size(); ++i) {
    if (i > 0)
      out << ", ";
    out << "{\"id\": " << g.nodes[i].id
        << ", \"originalId\": " << g.nodes[i].originalId
        << ", \"storedValue\": " << g.nodes[i].storedValue
        << ", \"valuableData\": "
        << (g.nodes[i].valuableData ? "true" : "false") << "}";
  }
  out << "],\n";
  out << "  \"edges\": [";
  for (size_t i = 0; i < g.edges.size(); ++i) {
    if (i > 0)
      out << ", ";
    out << "{\"fromId\": " << g.edges[i].fromId
        << ", \"toId\": " << g.edges[i].toId
        << ", \"weight\": " << g.edges[i].weight
        << ", \"actionType\": " << g.edges[i].actionType << "}";
  }
  out << "]\n";
  out << "}\n";
  out.close();
}

std::string graphToJSON(const ExportedGraph &g) {
  std::ostringstream oss;
  oss << "{\n";
  oss << "  \"seed\": " << g.seed << ",\n";
  oss << "  \"numNodes\": " << g.numNodes << ",\n";
  oss << "  \"numEdges\": " << g.numEdges << ",\n";
  oss << "  \"nodes\": [";
  for (size_t i = 0; i < g.nodes.size(); ++i) {
    if (i > 0)
      oss << ", ";
    oss << "{\"id\": " << g.nodes[i].id
        << ", \"originalId\": " << g.nodes[i].originalId
        << ", \"storedValue\": " << g.nodes[i].storedValue
        << ", \"valuableData\": "
        << (g.nodes[i].valuableData ? "true" : "false") << "}";
  }
  oss << "],\n";
  oss << "  \"edges\": [";
  for (size_t i = 0; i < g.edges.size(); ++i) {
    if (i > 0)
      oss << ", ";
    oss << "{\"fromId\": " << g.edges[i].fromId
        << ", \"toId\": " << g.edges[i].toId
        << ", \"weight\": " << g.edges[i].weight
        << ", \"actionType\": " << g.edges[i].actionType << "}";
  }
  oss << "]\n";
  oss << "}\n";
  return oss.str();
}
