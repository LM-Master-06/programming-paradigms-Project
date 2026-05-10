# programming-paradigms-Project

A high-distinction parallel computing project demonstrating advanced parallel patterns using C++, OpenMP, and MPI. Built as part of Deakin University's SIT315 — Concurrent and Distributed Programming, this project explores shared-memory and distributed-memory parallelism through performance-benchmarked implementations.

# ⚡ SIT315 — Parallel Patterns & Advanced Topics (M4.T1D)

[![C++](https://img.shields.io/badge/Language-C%2B%2B-00599C?logo=cplusplus&logoColor=white)](https://isocpp.org) [![OpenMP](https://img.shields.io/badge/Parallel-OpenMP-brightgreen)](https://www.openmp.org) [![MPI](https://img.shields.io/badge/Distributed-MPI-blue)](https://www.mpi-forum.org) [![Makefile](https://img.shields.io/badge/Build-Makefile-lightgrey)](https://www.gnu.org/software/make/) [![Shell](https://img.shields.io/badge/Scripts-Bash-4EAA25?logo=gnubash&logoColor=white)](https://www.gnu.org/software/bash/) [![Deakin](https://img.shields.io/badge/Deakin-SIT315-D0021B)](https://www.deakin.edu.au) [![Status](https://img.shields.io/badge/Status-Complete-2ECC71)](https://github.com/LM-Master-06/programming-paradigms-Project)

A **high-distinction parallel computing project** implementing advanced parallel patterns in C++ using OpenMP for shared-memory parallelism and MPI for distributed-memory parallelism.  
This project measures, compares, and analyses the performance of sequential and parallel implementations across multiple thread and process counts.

---

# 🚀 Features

## 🧵 Shared-Memory Parallelism (OpenMP)

- Multi-threaded parallel execution using OpenMP directives
- Fork-join parallelism with configurable thread counts
- Loop-level parallelism with `#pragma omp parallel for`
- Thread-private variables and critical section management
- Scalability testing across 1, 2, 4, and 8 threads

## 🌐 Distributed-Memory Parallelism (MPI)

- Multi-process execution across distributed nodes
- Work distribution using `MPI_Scatter` and result collection via `MPI_Gather`
- Collective communication operations
- Rank-based task assignment for load balancing
- Configurable process counts for scalability benchmarking

## 📊 Performance Benchmarking

- Sequential baseline vs parallel execution time comparison
- Speedup and efficiency calculations across thread/process counts
- Shell scripts for automated benchmark runs
- Timing measured using high-resolution C++ chrono library

## 🏗️ Clean Build System

- Makefile-driven compilation with separate targets for each implementation
- Compile flags for OpenMP (`-fopenmp`) and MPI (`mpicxx`) configured automatically
- Single `make all` command builds all variants

---

# 🏗️ Tech Stack

## Core

- C++17
- OpenMP (shared-memory parallelism)
- MPI / Open MPI (distributed-memory parallelism)
- GNU Make (build system)
- Bash (benchmark automation scripts)

## Tools & Environment

- GCC / g++ compiler
- `mpirun` / `mpiexec` for MPI process launch
- Deakin SIT315 virtual machine environment

---

# 🧱 Parallel Architecture

## OpenMP (Shared Memory)

```
Master Thread
↓
Fork: spawn N worker threads
↓
Parallel work region (loop / task)
↓
Join: synchronise results
↓
Output result
```

## MPI (Distributed Memory)

```
Process 0 (Root)
↓
MPI_Scatter → distribute work to N processes
↓
Each process computes its local result
↓
MPI_Gather → collect results at root
↓
Root outputs final result
```

---

# 📂 Project Structure

```
programming-paradigms-Project/
├── code/                           # All C++ source implementations
│   ├── sequential.cpp              # Baseline sequential implementation
│   ├── openmp.cpp                  # OpenMP parallel implementation
│   ├── mpi.cpp                     # MPI distributed implementation
│   ├── Makefile                    # Build configuration
│   └── run_benchmark.sh            # Automated benchmark script
├── SIT315 Task M4.T1D.docx         # Task specification (Word)
├── SIT315 Task M4.T1D.pdf          # Task specification (PDF)
└── SIT315 video link.pdf           # Video demonstration link
```

---

# ⚙️ Installation & Setup

## 🔧 Prerequisites

```bash
# Install GCC and OpenMP (Linux)
sudo apt-get install g++ libomp-dev

# Install Open MPI
sudo apt-get install openmpi-bin libopenmpi-dev
```

---

## 🏗️ Build

```bash
git clone https://github.com/LM-Master-06/programming-paradigms-Project.git
cd programming-paradigms-Project/code

# Build all implementations
make all

# Or build individually
make sequential
make openmp
make mpi
```

---

## ▶️ Run

### Sequential (baseline)

```bash
./sequential
```

### OpenMP (shared-memory parallel)

```bash
# Set thread count via environment variable
export OMP_NUM_THREADS=4
./openmp
```

### MPI (distributed-memory parallel)

```bash
# Run with 4 processes
mpirun -np 4 ./mpi
```

---

## 📊 Run Benchmarks

```bash
chmod +x run_benchmark.sh
./run_benchmark.sh
```

The script runs all implementations across multiple thread/process counts and outputs execution times for comparison.

---

# 📈 Performance Analysis

| Implementation | Threads / Processes | Speedup |
|---|---|---|
| Sequential | 1 | 1.0× (baseline) |
| OpenMP | 2 | ~1.8× |
| OpenMP | 4 | ~3.2× |
| OpenMP | 8 | ~5.5× |
| MPI | 2 | ~1.7× |
| MPI | 4 | ~3.0× |

> Actual results vary based on hardware. See the task report (`SIT315 Task M4.T1D.pdf`) for full benchmarking data and analysis.

---

# 🔒 Key Concepts Demonstrated

- Race condition avoidance with OpenMP critical sections and atomic operations
- Load balancing across MPI ranks
- Amdahl's Law and theoretical speedup limits
- Memory locality and cache effects in parallel loops
- Synchronisation overhead analysis

---

# 📌 Future Enhancements

- Hybrid MPI + OpenMP implementation for multi-node shared-memory clusters
- CUDA GPU parallelisation for data-parallel workloads
- Apache Spark / MapReduce implementation for cloud-scale data
- Automated performance visualisation (speedup / efficiency plots)

---

# 🤝 Contribution

Fork → Create Branch → Commit → Push → PR

---

# 📄 License

MIT License

---

# 👨‍💻 Author

LM-Master-06  
