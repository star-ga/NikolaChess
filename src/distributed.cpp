// Distributed search implementation for Supercomputer Chess Engine.
//
// Copyright (c) 2025 CPUTER Inc.
// All rights reserved.  See the LICENSE file for license terms.
//
// This file provides a stub implementation of a distributed search
// function.  In a build configured with MPI support, calls to
// distributed_search() will initialise MPI, coordinate search
// across ranks and finalise the MPI environment.  Without MPI,
// the function simply returns immediately.  Real implementations
// should broadcast board positions, collect results and manage
// distributed transposition tables.

#include "distributed.h"

// If MPI support is enabled, include the MPI header.  Users can
// define NIKOLA_USE_MPI in the build system to enable this path.
#ifdef NIKOLA_USE_MPI
#include <mpi.h>
#endif

namespace nikola {

int distributed_search() {
#ifdef NIKOLA_USE_MPI
    int provided;
    // Initialise MPI with support for multiple threads if available.
    MPI_Init_thread(nullptr, nullptr, MPI_THREAD_SERIALIZED, &provided);
    int rank = 0;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    int size = 1;
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    // TODO: Implement distributed search logic here.  This stub
    // currently does nothing and simply prints the rank and size.
    // A full implementation would partition the search tree among
    // ranks, broadcast root positions and reduce evaluation results.
    // After the search completes, finalise MPI.
    MPI_Finalize();
    (void)rank;
    (void)size;
    return 0;
#else
    // MPI is not enabled; distributed search is unavailable.
    return 0;
#endif
}

} // namespace nikola