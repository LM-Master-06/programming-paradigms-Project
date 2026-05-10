// =============================================================================
//  nbody_hybrid.cpp
//  Hybrid MPI + OpenMP N-Body Gravitational Simulation
//  with Barnes-Hut Octree (O(N log N)) acceleration
//
//  SIT315 – Task M4.T1D  |  Level 3 (High HD) Project
//
//  Theme++: Barnes-Hut tree is NOT covered in unit materials.
//  It reduces force computation from O(N²) to O(N log N) and is combined
//  with MPI domain decomposition and OpenMP thread-level parallelism.
//
//  Compile:
//    mpicxx -O2 -std=c++14 -o nbody_hybrid nbody_hybrid.cpp -lm
//
//  Run examples:
//    mpirun -np 4 ./nbody_hybrid 1000  20   # N=1000, 20 steps
//    mpirun -np 4 ./nbody_hybrid 5000  20   # N=5000, 20 steps
//    mpirun -np 4 ./nbody_hybrid 10000 20   # N=10000, 20 steps
//
//  Sequential baseline (1 MPI rank, 1 thread):
//    OMP_NUM_THREADS=1 mpirun -np 1 ./nbody_hybrid 1000 20
// =============================================================================

#include <mpi.h>

#include <cmath>
#include <vector>
#include <random>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <numeric>
#include <cstring>
#include <cassert>
#include <chrono>

// ─── Physical / simulation constants ─────────────────────────────────────────
static const double G          = 6.674e-11;   // gravitational constant (SI)
static const double SOFTENING  = 1.0e9;        // softening length in metres
static const double THETA      = 0.5;          // Barnes-Hut opening angle (0 = exact, 1 = coarser)

// ─── Vec3: simple 3-D vector ──────────────────────────────────────────────────
struct Vec3 {
    double x{0.0}, y{0.0}, z{0.0};

    Vec3 operator+(const Vec3& o) const { return {x+o.x, y+o.y, z+o.z}; }
    Vec3 operator-(const Vec3& o) const { return {x-o.x, y-o.y, z-o.z}; }
    Vec3 operator*(double s)     const { return {x*s,   y*s,   z*s  }; }
    Vec3& operator+=(const Vec3& o){ x+=o.x; y+=o.y; z+=o.z; return *this; }
    double norm2() const { return x*x + y*y + z*z; }
};

// ─── Body ─────────────────────────────────────────────────────────────────────
struct Body {
    Vec3   pos, vel;
    double mass{1.0};
};

// ─── Octree node ──────────────────────────────────────────────────────────────
// Each node covers a cubic region centered at `center` with half-width `half`.
// Leaf nodes store one body (bodyIdx >= 0).  Internal nodes have bodyIdx = -1.
struct OctNode {
    Vec3   center{};
    double half{0.0};
    double mass{0.0};
    Vec3   com{};          // aggregate centre of mass
    int    child[8];       // child indices into pool; -1 = absent
    int    bodyIdx{-1};    // >= 0 only for occupied leaves

    OctNode() { std::fill(child, child+8, -1); }
};

// ─── Octree ───────────────────────────────────────────────────────────────────
// Built fresh every timestep from the FULL body array (all ranks share it).
// Force queries are then parallelised with OpenMP.
class Octree {
    std::vector<OctNode>    pool_;
    const std::vector<Body>& bodies_;

    // Which octant of `cen` does point `p` fall in?
    static int octant(const Vec3& cen, const Vec3& p) {
        return ((p.x >= cen.x) ? 1 : 0)
             | ((p.y >= cen.y) ? 2 : 0)
             | ((p.z >= cen.z) ? 4 : 0);
    }

    // Centre of child octant `oct` (half-side `half`, parent centre `cen`)
    static Vec3 childCen(const Vec3& cen, double half, int oct) {
        const double q = half * 0.5;
        return { cen.x + ((oct & 1) ? q : -q),
                 cen.y + ((oct & 2) ? q : -q),
                 cen.z + ((oct & 4) ? q : -q) };
    }

    // Create child node in given octant of ni, return its index.
    int makeChild(int ni, int oct) {
        OctNode ch;
        ch.center = childCen(pool_[ni].center, pool_[ni].half, oct);
        ch.half   = pool_[ni].half * 0.5;
        int idx   = static_cast<int>(pool_.size());
        pool_.push_back(ch);            // may invalidate refs → use indices
        pool_[ni].child[oct] = idx;
        return idx;
    }

    // Insert body bi into the node at index ni.
    // COM and mass of ni are updated here; children are updated by recursion.
    void insert(int ni, int bi) {
        OctNode& n = pool_[ni];

        // ── Empty leaf: just store the body ──────────────────────────────────
        if (n.mass == 0.0) {
            n.bodyIdx = bi;
            n.mass    = bodies_[bi].mass;
            n.com     = bodies_[bi].pos;
            return;
        }

        // ── Update aggregate COM and mass of this node ────────────────────────
        const double mB     = bodies_[bi].mass;
        const double newM   = n.mass + mB;
        n.com.x = (n.com.x * n.mass + bodies_[bi].pos.x * mB) / newM;
        n.com.y = (n.com.y * n.mass + bodies_[bi].pos.y * mB) / newM;
        n.com.z = (n.com.z * n.mass + bodies_[bi].pos.z * mB) / newM;
        n.mass  = newM;

        // ── Occupied leaf → subdivide ─────────────────────────────────────────
        if (n.bodyIdx >= 0) {
            int exBi   = n.bodyIdx;
            n.bodyIdx  = -1;              // convert to internal node
            // Push existing body into the appropriate child
            int octEx  = octant(pool_[ni].center, bodies_[exBi].pos);
            if (pool_[ni].child[octEx] == -1) makeChild(ni, octEx);
            insert(pool_[ni].child[octEx], exBi);
        }

        // ── Push new body into appropriate child ──────────────────────────────
        int octBi = octant(pool_[ni].center, bodies_[bi].pos);
        if (pool_[ni].child[octBi] == -1) makeChild(ni, octBi);
        insert(pool_[ni].child[octBi], bi);
    }
    // Compute gravitational force on body bi from the subtree rooted at ni.
    Vec3 forceOn(int ni, int bi) const {
        const OctNode& nd = pool_[ni];
        if (nd.mass == 0.0) return {};

        const Vec3   diff  = { nd.com.x - bodies_[bi].pos.x,
                                nd.com.y - bodies_[bi].pos.y,
                                nd.com.z - bodies_[bi].pos.z };
        const double dist2 = diff.norm2() + SOFTENING * SOFTENING;
        const double dist  = std::sqrt(dist2);

        // ── Leaf node ─────────────────────────────────────────────────────────
        if (nd.bodyIdx >= 0) {
            if (nd.bodyIdx == bi) return {};   // skip self
            const double mag = G * bodies_[bi].mass * nd.mass / dist2;
            return diff * (mag / dist);
        }

        // ── Barnes-Hut criterion: s/d < θ → treat as single particle ─────────
        if ((2.0 * nd.half) / dist < THETA) {
            const double mag = G * bodies_[bi].mass * nd.mass / dist2;
            return diff * (mag / dist);
        }

        // ── Recurse into children ─────────────────────────────────────────────
        Vec3 f{};
        for (int c : nd.child)
            if (c != -1) f += forceOn(c, bi);
        return f;
    }

public:
    explicit Octree(const std::vector<Body>& b) : bodies_(b) {}

    // Build tree from scratch using all bodies.
    void build() {
        pool_.clear();
        if (bodies_.empty()) return;

        // Bounding box
        double minX = bodies_[0].pos.x, maxX = minX;
        double minY = bodies_[0].pos.y, maxY = minY;
        double minZ = bodies_[0].pos.z, maxZ = minZ;
        for (const auto& b : bodies_) {
            minX = std::min(minX, b.pos.x); maxX = std::max(maxX, b.pos.x);
            minY = std::min(minY, b.pos.y); maxY = std::max(maxY, b.pos.y);
            minZ = std::min(minZ, b.pos.z); maxZ = std::max(maxZ, b.pos.z);
        }
        const double half = std::max({maxX-minX, maxY-minY, maxZ-minZ}) * 0.505;

        OctNode root;
        root.center = { (minX+maxX)*0.5, (minY+maxY)*0.5, (minZ+maxZ)*0.5 };
        root.half   = half;
        pool_.reserve(bodies_.size() * 10);   // typical tree has ~8–10× nodes
        pool_.push_back(root);

        for (int i = 0; i < static_cast<int>(bodies_.size()); ++i)
            insert(0, i);
    }

    // Return gravitational force on body bi (uses entire tree).
    Vec3 force(int bi) const {
        return pool_.empty() ? Vec3{} : forceOn(0, bi);
    }
};

// ─── Utility: wall-clock timer ────────────────────────────────────────────────
static double wallTime() { return MPI_Wtime(); }

// ─── Initialise bodies (called on rank 0, then broadcast) ────────────────────
static void initBodies(std::vector<Body>& bodies, unsigned seed) {
    std::mt19937_64 rng(seed);
    std::uniform_real_distribution<double> pos(-1.0e11, 1.0e11);  // ±100 AU
    std::uniform_real_distribution<double> vel(-1.0e3,  1.0e3 );  // ±1 km/s
    std::uniform_real_distribution<double> mass(1.0e20, 1.0e30);  // tiny → stellar

    for (auto& b : bodies) {
        b.pos  = { pos(rng), pos(rng), pos(rng) };
        b.vel  = { vel(rng), vel(rng), vel(rng) };
        b.mass = mass(rng);
    }
}

// ─── MPI flat-array helpers ───────────────────────────────────────────────────
// Each body is packed as 7 doubles: px py pz vx vy vz mass
static const int BODY_DOUBLES = 7;

static void bodiesToFlat(const std::vector<Body>& bodies, std::vector<double>& flat) {
    flat.resize(bodies.size() * BODY_DOUBLES);
    for (int i = 0; i < static_cast<int>(bodies.size()); ++i) {
        flat[i*BODY_DOUBLES+0] = bodies[i].pos.x;
        flat[i*BODY_DOUBLES+1] = bodies[i].pos.y;
        flat[i*BODY_DOUBLES+2] = bodies[i].pos.z;
        flat[i*BODY_DOUBLES+3] = bodies[i].vel.x;
        flat[i*BODY_DOUBLES+4] = bodies[i].vel.y;
        flat[i*BODY_DOUBLES+5] = bodies[i].vel.z;
        flat[i*BODY_DOUBLES+6] = bodies[i].mass;
    }
}

static void flatToBodies(const std::vector<double>& flat, std::vector<Body>& bodies) {
    for (int i = 0; i < static_cast<int>(bodies.size()); ++i) {
        bodies[i].pos  = { flat[i*BODY_DOUBLES+0], flat[i*BODY_DOUBLES+1], flat[i*BODY_DOUBLES+2] };
        bodies[i].vel  = { flat[i*BODY_DOUBLES+3], flat[i*BODY_DOUBLES+4], flat[i*BODY_DOUBLES+5] };
        bodies[i].mass =   flat[i*BODY_DOUBLES+6];
    }
}

// ─── Main ─────────────────────────────────────────────────────────────────────
int main(int argc, char* argv[]) {

    // ── MPI initialisation ────────────────────────────────────────────────────
    int provided;
    MPI_Init_thread(&argc, &argv, MPI_THREAD_FUNNELED, &provided);
    int rank, nprocs;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);

    // ── Command-line arguments ────────────────────────────────────────────────
    int    N     = (argc >= 2) ? std::atoi(argv[1]) : 1000;
    int    steps = (argc >= 3) ? std::atoi(argv[2]) : 20;
    double dt    = (argc >= 4) ? std::atof(argv[3]) : 1.0e6;  // timestep (s)
    int    seed  = (argc >= 5) ? std::atoi(argv[4]) : 42;

    if (N < nprocs) {
        if (rank == 0)
            std::cerr << "Error: N (" << N << ") must be >= nprocs (" << nprocs << ")\n";
        MPI_Finalize(); return 1;
    }

    char processorName[MPI_MAX_PROCESSOR_NAME];
    int processorNameLen = 0;
    MPI_Get_processor_name(processorName, &processorNameLen);
    processorName[processorNameLen] = '\0';
    const int threadId = 0;

    if (rank == 0) {
        std::cout << "========================================\n"
                  << "  Hybrid MPI+OpenMP N-Body (Barnes-Hut)\n"
                  << "========================================\n"
                  << "  N        = " << N       << " bodies\n"
                  << "  Steps    = " << steps   << "\n"
                  << "  dt       = " << dt      << " s\n"
                  << "  MPI ranks= " << nprocs  << "\n"
                  << "  Rank     = " << rank     << " on " << processorName << "\n"
                  << "  Thread   = " << threadId << "\n"
                  << "  Threads  = 1\n"
                  << "  theta    = " << THETA   << "\n"
                  << "  softening= " << SOFTENING << " m\n"
                  << "----------------------------------------\n";
    }

    // ── Initialise all bodies on rank 0 and broadcast ────────────────────────
    std::vector<Body>   bodies(N);
    std::vector<double> flatAll(N * BODY_DOUBLES);

    if (rank == 0) {
        initBodies(bodies, static_cast<unsigned>(seed));
        bodiesToFlat(bodies, flatAll);
    }
    
    
    MPI_Bcast(flatAll.data(), N * BODY_DOUBLES, MPI_DOUBLE, 0, MPI_COMM_WORLD);
    if (rank != 0) flatToBodies(flatAll, bodies);

    // ── Domain decomposition ──────────────────────────────────────────────────
    // Rank r is responsible for bodies [ localStart[r], localEnd[r] )
    std::vector<int> localStart(nprocs), localEnd(nprocs), localCount(nprocs);
    for (int r = 0; r < nprocs; ++r) {
        localStart[r] = r * N / nprocs;
        localEnd[r]   = (r+1) * N / nprocs;
        localCount[r] = localEnd[r] - localStart[r];
    }
    const int myStart = localStart[rank];
    (void)localEnd[rank];              // bounds used via myN only
    const int myN     = localCount[rank];

    // Allgather displacement and count arrays (for MPI_Allgatherv)
    std::vector<int> gCounts(nprocs), gDispls(nprocs);
    for (int r = 0; r < nprocs; ++r) {
        gCounts[r] = localCount[r] * BODY_DOUBLES;
        gDispls[r] = localStart[r] * BODY_DOUBLES;
    }

    std::vector<double> localFlat(myN * BODY_DOUBLES);
    std::vector<Vec3>   forces(myN);

    // ── Timing: synchronise and record ───────────────────────────────────────
    MPI_Barrier(MPI_COMM_WORLD);
    const double t0 = wallTime();

    // ── Main simulation loop ──────────────────────────────────────────────────
    for (int step = 0; step < steps; ++step) {

        // 1. Build Barnes-Hut octree on EVERY rank from ALL bodies.
        //    This avoids communicating the tree; tree construction is cheap
        //    relative to force computation for large N.
        Octree tree(bodies);
        tree.build();

        // 2. Compute forces on LOCAL bodies.
        for (int i = 0; i < myN; ++i) {
            forces[i] = tree.force(myStart + i);
        }

        // 3. Leapfrog integration: update velocity and position.
        for (int i = 0; i < myN; ++i) {
            const int  bi  = myStart + i;
            const double m = bodies[bi].mass;
            bodies[bi].vel += forces[i] * (dt / m);
            bodies[bi].pos += bodies[bi].vel * dt;
        }

        // 4. Prepare local flat state for communication.
        for (int i = 0; i < myN; ++i) {
            const int bi = myStart + i;
            localFlat[i*BODY_DOUBLES+0] = bodies[bi].pos.x;
            localFlat[i*BODY_DOUBLES+1] = bodies[bi].pos.y;
            localFlat[i*BODY_DOUBLES+2] = bodies[bi].pos.z;
            localFlat[i*BODY_DOUBLES+3] = bodies[bi].vel.x;
            localFlat[i*BODY_DOUBLES+4] = bodies[bi].vel.y;
            localFlat[i*BODY_DOUBLES+5] = bodies[bi].vel.z;
            localFlat[i*BODY_DOUBLES+6] = bodies[bi].mass;
        }

        // 5. All-to-all gather: share updated body states.
        MPI_Allgatherv(localFlat.data(), myN * BODY_DOUBLES, MPI_DOUBLE,
                       flatAll.data(),   gCounts.data(), gDispls.data(), MPI_DOUBLE,
                       MPI_COMM_WORLD);

        // 6. Update ALL bodies from gathered state so tree next step is correct.
        flatToBodies(flatAll, bodies);

        if (rank == 0 && (step % 5 == 0 || step == steps-1)) {
            std::cout << "  [" << processorName << ":rank " << rank
                      << ", thread " << threadId << "] Step "
                      << std::setw(4) << step+1
                      << "/" << steps
                      << "  elapsed=" << std::fixed << std::setprecision(3)
                      << (wallTime() - t0) << "s\n";
        }
    }

    // ── Report total time ─────────────────────────────────────────────────────
    MPI_Barrier(MPI_COMM_WORLD);
    const double elapsed = wallTime() - t0;

    if (rank == 0) {
        std::cout << "----------------------------------------\n"
                  << "  TOTAL TIME : " << std::fixed << std::setprecision(4)
                  << elapsed << " s\n"
                  << "  Per step   : " << (elapsed / steps) << " s\n"
                  << "========================================\n";

        // Append result to CSV for evaluation plotting
        std::ofstream csv("results.csv", std::ios::app);
        if (csv.is_open()) {
            csv << N << "," << nprocs << "," << 1 << ","
                << steps << "," << std::fixed << std::setprecision(4)
                << elapsed << "\n";
        }
    }
    // for (int i = 0; i < N; i++)
    // {
    //     std::cout << "Body " << i << ": pos=("
    //               << flatAll[i*BODY_DOUBLES+0] << ", "
    //               << flatAll[i*BODY_DOUBLES+1] << ", "
    //               << flatAll[i*BODY_DOUBLES+2] << ") vel=("
    //               << flatAll[i*BODY_DOUBLES+3] << ", "
    //               << flatAll[i*BODY_DOUBLES+4] << ", "
    //               << flatAll[i*BODY_DOUBLES+5] << ") mass="
    //               << flatAll[i*BODY_DOUBLES+6] << "\n";
    // }
    

    MPI_Finalize();
    return 0;
}
