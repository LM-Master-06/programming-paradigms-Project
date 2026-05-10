# =============================================================================
#  Makefile – Hybrid MPI+OpenMP N-Body (Barnes-Hut)
#  SIT315 Task M4.T1D
# =============================================================================

CXX      = mpicxx
CXXFLAGS = -O2 -fopenmp -std=c++17 -Wall -Wextra
TARGET   = nbody_hybrid

.PHONY: all clean run test

all: $(TARGET)

$(TARGET): nbody_hybrid.cpp
	$(CXX) $(CXXFLAGS) -o $@ $< -lm
	@echo "Build successful: ./$(TARGET)"

# Quick smoke test: 4 ranks, 500 bodies, 5 steps
run: $(TARGET)
	OMP_NUM_THREADS=2 mpirun -np 4 ./$(TARGET) 500 5

# Full evaluation suite (writes results.csv)
test: $(TARGET)
	@echo "N,ranks,threads,steps,time_s" > results.csv
	@echo "=== Sequential baseline (1 rank, 1 thread) ==="
	OMP_NUM_THREADS=1 mpirun -np 1 ./$(TARGET) 1000  20
	OMP_NUM_THREADS=1 mpirun -np 1 ./$(TARGET) 5000  20
	OMP_NUM_THREADS=1 mpirun -np 1 ./$(TARGET) 10000 20
	@echo ""
	@echo "=== 2 MPI ranks × 2 OMP threads ==="
	OMP_NUM_THREADS=2 mpirun -np 2 ./$(TARGET) 1000  20
	OMP_NUM_THREADS=2 mpirun -np 2 ./$(TARGET) 5000  20
	OMP_NUM_THREADS=2 mpirun -np 2 ./$(TARGET) 10000 20
	@echo ""
	@echo "=== 4 MPI ranks × 2 OMP threads ==="
	OMP_NUM_THREADS=2 mpirun -np 4 ./$(TARGET) 1000  20
	OMP_NUM_THREADS=2 mpirun -np 4 ./$(TARGET) 5000  20
	OMP_NUM_THREADS=2 mpirun -np 4 ./$(TARGET) 10000 20
	@echo ""
	@echo "Results written to results.csv"

clean:
	rm -f $(TARGET) results.csv
