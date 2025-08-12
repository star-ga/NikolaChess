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
#include <mutex>
#include <string>

// Fathom tablebase library entry point.  Returns non-zero on success.
extern "C" int tb_init(const char* path);

// Probe wrapper implemented in tbprobe.cpp.
namespace nikola { unsigned tbProbeWDL(const Board& b); }

namespace nikola {

// Store the path to the tablebase files.  Accesses are protected
// by a mutex to allow thread‑safe updates and reads.
static std::string g_tbPath;
static bool g_tbAvailable = false;
static int g_tbPathUpdates = 0;
static std::mutex g_tbMutex;

static int countPiecesLocal(const Board& b) {
    int count = 0;
    for (int r = 0; r < 8; ++r)
        for (int c = 0; c < 8; ++c)
            if (b.squares[r][c] != EMPTY) ++count;
    return count;
}

void setTablebasePath(const std::string& path) {
    std::lock_guard<std::mutex> lock(g_tbMutex);
    g_tbPath = path;
    g_tbAvailable = false;
    if (!path.empty()) {
        g_tbAvailable = tb_init(path.c_str()) != 0;
    }
    ++g_tbPathUpdates;
}

bool tablebaseAvailable() {
    std::lock_guard<std::mutex> lock(g_tbMutex);
    return g_tbAvailable;
}

std::string currentTablebasePath() {
    std::lock_guard<std::mutex> lock(g_tbMutex);
    return g_tbPath;
}

int tablebasePathUpdateCount() {
    std::lock_guard<std::mutex> lock(g_tbMutex);
    return g_tbPathUpdates;
}

int probeWDL(const Board& board) {
    if (!tablebaseAvailable()) {
        return 2;
    }
    // For now we rely on Fathom which supports up to 7 pieces.
    if (countPiecesLocal(board) > 7) {
        return 2;
    }
    unsigned res = tbProbeWDL(board);
    switch (res) {
        case 4: return 1;   // win
        case 2: return 0;   // draw
        case 0: return -1;  // loss
        default: return 2;  // unknown
    }
}

} // namespace nikola