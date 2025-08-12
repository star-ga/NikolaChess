// Distributed search interface for Supercomputer Chess Engine.
//
// This header declares functions to coordinate search across multiple
// processes using MPI.  When compiled with MPI support, the
// implementation will distribute the search tree across nodes in a
// cluster.  When MPI is not available, these functions fall back to
// local implementations.

#pragma once

namespace nikola {

// Perform a distributed search across multiple processes.  The
// details of the search (board position, depth, etc.) should be
// passed through global state or additional parameters in a fuller
// implementation.  Returns an integer status code (0 for success).
int distributed_search();

} // namespace nikola
