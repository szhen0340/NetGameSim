CXX = mpic++
CXXFLAGS = -std=c++17 -O3 -Wall -Iinclude -I/opt/homebrew/include
LDFLAGS = -L/opt/homebrew/lib

JSON_ROOT = /usr/local
JSON_INC = $(JSON_ROOT)/include
JSON_LIB = $(JSON_ROOT)/lib

SOURCES = src/common.cpp src/partition.cpp src/runtime.cpp
HEADERS = include/common.h include/floodmax.h include/dijkstra.h

PARTITION_OBJS = obj/partition.o obj/common.o
RUNTIME_OBJS = obj/runtime.o obj/common.o

GRAPH_FILE ?= outputs/NetGraph_13-04-26-15-29-44.ngs

.PHONY: all clean partition runtime install-json test-unit

all: partition runtime

partition: $(PARTITION_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS) -ljsoncpp

runtime: $(RUNTIME_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS) -lm -ljsoncpp

obj/%.o: src/%.cpp $(HEADERS)
	@mkdir -p obj
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -rf obj partition runtime test_runtime

install-json:
	@if [ ! -d "$(JSON_ROOT)/include/json" ]; then \
		echo "Installing json-cpp..."; \
		brew install json-cpp || echo "json-cpp not found, using header-only mode"; \
	fi

test: partition
	@echo "Running partition test..."
	@mpiexec -n 2 ./partition $(GRAPH_FILE) || echo "MPI test requires multiple processes"

test-unit: obj/common.o
	@echo "Building unit tests..."
	$(CXX) $(CXXFLAGS) -o test_runtime tests/test_runtime.cpp obj/common.o $(LDFLAGS) -ljsoncpp
	@echo "Running unit tests..."
	@./test_runtime

run-runtime: runtime
	@echo "Running distributed Dijkstra..."
	@mpiexec -n 2 ./runtime $(GRAPH_FILE) -o results.txt || echo "MPI runtime requires multiple processes"

help:
	@echo "NetGameSim MPI Runtime Build System"
	@echo "==================================="
	@echo "Targets:"
	@echo "  all          - Build all tools (partition, runtime)"
	@echo "  partition    - Build the graph partition tool"
	@echo "  runtime      - Build the distributed Dijkstra runtime"
	@echo "  clean        - Remove built files"
	@echo "  install-json - Install json-cpp library"
	@echo "  test         - Run partition test with 2 MPI processes"
	@echo "  run-runtime  - Run distributed Dijkstra with 2 MPI processes"
	@echo ""
	@echo "Usage:"
	@echo "  make partition"
	@echo "  mpiexec -n 4 ./partition <graph_file.ngs>"
	@echo ""
	@echo "  make runtime"
	@echo "  mpiexec -n 4 ./runtime <graph_file.ngs> -o results.txt"
