#!/bin/bash
# =============================================================================
#  run_evaluation.sh
#  Evaluation script for SIT315 Task M4.T1D
#  Records timing for 3 workload sizes × 3 parallelism configurations.
#  Results appended to results.csv.
# =============================================================================

set -e

BINARY="./nbody_hybrid"
STEPS=20
RESULTS="results.csv"

if [ ! -f "$BINARY" ]; then
    echo "Binary not found. Run 'make' first."
    exit 1
fi

echo "N,ranks,threads,steps,time_s" > "$RESULTS"

run_case() {
    local N=$1
    local RANKS=$2
    local THREADS=$3
    echo "  Running N=$N  ranks=$RANKS  threads=$THREADS ..."
    OMP_NUM_THREADS=$THREADS mpirun --oversubscribe -np $RANKS $BINARY $N $STEPS
}

echo "============================================================"
echo "  Evaluation: Hybrid MPI+OpenMP N-Body Barnes-Hut"
echo "  Workloads  : N = 1000, 5000, 10000"
echo "  Steps      : $STEPS"
echo "============================================================"

echo ""
echo "--- Config 1: Sequential (1 rank, 1 thread) ---"
for N in 1000 5000 10000; do
    run_case $N 1 1
done

echo ""
echo "--- Config 2: 2 MPI ranks × 2 OMP threads ---"
for N in 1000 5000 10000; do
    run_case $N 2 2
done

echo ""
echo "--- Config 3: 4 MPI ranks × 2 OMP threads ---"
for N in 1000 5000 10000; do
    run_case $N 4 2
done

echo ""
echo "============================================================"
echo "  All results saved to: $RESULTS"
echo "============================================================"
cat "$RESULTS"
