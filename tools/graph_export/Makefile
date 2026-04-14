CXX = g++
CXXFLAGS = -std=c++17 -O3 -Wall -Iinclude -I/opt/homebrew/include
LDFLAGS = -L/opt/homebrew/lib

SOURCES = src/main.cpp src/graph_export.cpp
HEADERS = include/graph_export.h

graph_export: $(SOURCES) $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $@ $(SOURCES) $(LDFLAGS) -ljsoncpp

clean:
	rm -f graph_export

test: graph_export
	@echo "Testing graph_export..."
	@if [ -z "$(GRAPH_FILE)" ]; then \
		echo "GRAPH_FILE must be specified: make test GRAPH_FILE=../../outputs/NetGraph.ngs"; \
		exit 1; \
	fi
	@./graph_export $(GRAPH_FILE) -o /tmp/test_export.json
	@echo "Export test passed"

help:
	@echo "Graph Export Tool"
	@echo "================="
	@echo "Usage: ./graph_export <input_file> [options]"
	@echo ""
	@echo "Options:"
	@echo "  -o <file>   Output file (default: stdout)"
	@echo "  -s <seed>    Override random seed"
	@echo ""
	@echo "Examples:"
	@echo "  ./graph_export input.ngs -o enriched.json"
	@echo "  ./graph_export input.ngs -s 42 -o reproducible.json"
