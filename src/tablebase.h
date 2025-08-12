// Copyright (c) 2025 CPUTER Inc.
//
// This header declares a minimal interface for probing endgame
// tablebases.  At present this is a stub that always reports
// "unknown".  In future versions this interface may be wired up
// to Syzygy or Lomonosov tablebase probing code, allowing the
// engine to return perfect information in positions with a
// sufficiently small number of pieces.

#pragma once

#include <string>
#include "board.h"

namespace nikola {

// Set the path to the directory containing tablebase files.  The
// path should point to a collection of Syzygy WDL/DTZ files or
// other endgame databases.  Calling this function with a non‑empty
// path enables tablebase probing; passing an empty string disables
// it.  A future implementation may verify that the path contains
// valid tablebase files.
void setTablebasePath(const std::string& path);

// Return true if tablebase probing is enabled and at least one
// tablebase file has been detected.  In this stub implementation
// this simply returns whether a non‑empty path has been set.
bool tablebaseAvailable();

// Probe the tablebase for the given board and return the WDL
// (win/draw/loss) result from the perspective of the side to move.
// The return value is one of:
//   1   – winning (checkmate can be forced)
//   0   – drawing (no forced win or loss)
//  -1   – losing (checkmate cannot be avoided)
//   2   – unknown or not available (either no tablebase loaded or
//         the position exceeds the supported number of pieces).
// In a full implementation this function would also consider
// stalemate and illegal positions.  For now it always returns 2.
int probeWDL(const Board& board);

} // namespace nikola