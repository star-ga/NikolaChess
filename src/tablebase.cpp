// Copyright (c) 2025 CPUTER Inc.
//
// Stub implementation of endgame tablebase probing.  This module
// stores a path to a directory where tablebase files may reside
// and provides functions to check availability and probe for
// Win/Draw/Loss (WDL) information.  At present no actual probing
// is performed; the probe function always returns 2 (unknown).
// Future versions may link against a Syzygy probing library or
// other tablebase back end to return accurate information when the
// position contains a small number of pieces.

#include "tablebase.h"
#include <string>
#include <mutex>

namespace nikola {

// Store the path to the tablebase files.  Accesses are protected
// by a mutex to allow thread‑safe updates and reads.
static std::string g_tbPath;
static bool g_tbAvailable = false;
static std::mutex g_tbMutex;

void setTablebasePath(const std::string& path) {
    std::lock_guard<std::mutex> lock(g_tbMutex);
    g_tbPath = path;
    // In this stub we simply record whether a non‑empty path has
    // been provided.  A real implementation would verify that
    // required tablebase files exist and set g_tbAvailable
    // accordingly.  The Lomonosov 7‑man tablebases, for example,
    // consist of multiple files per configuration.
    g_tbAvailable = !path.empty();
}

bool tablebaseAvailable() {
    std::lock_guard<std::mutex> lock(g_tbMutex);
    return g_tbAvailable;
}

int probeWDL(const Board& /*board*/) {
    // Always return unknown.  A future implementation will count
    // the number of pieces on the board, index into the relevant
    // tablebase and return win/draw/loss.  Syzygy WDL files encode
    // results as 0 (unknown), 1 (draw), 2 (win), 3 (loss) with
    // respect to the side to move.  Here we adopt a simpler
    // convention: +1 for White win, 0 draw, -1 for Black win,
    // 2 unknown.  Because we cannot probe anything in this stub
    // implementation we always return unknown.
    return 2;
}

} // namespace nikola