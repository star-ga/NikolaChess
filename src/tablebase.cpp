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
#include "board.h"
#include <mutex>
#include <string>
#include <unordered_map>

// Fathom tablebase library entry point.  Returns non-zero on success.
extern "C" int tb_init(const char* path);

// Probe wrapper implemented in tbprobe.cpp.
namespace nikola { unsigned tbProbeWDL(const Board& b); }
namespace nikola { int tbProbeDTZ(const Board& b); }

namespace nikola {

// Store the path to the tablebase files.  Accesses are protected
// by a mutex to allow thread‑safe updates and reads.
static std::string g_tbPath;
static bool g_tbAvailable = false;
static int g_tbPathUpdates = 0;
static std::mutex g_tbMutex;
static std::mutex g_cacheMutex;
static std::unordered_map<std::string, int> g_wdlCache;
static std::unordered_map<std::string, int> g_dtzCache;

void setTablebasePath(const std::string& path) {
    std::lock_guard<std::mutex> lock(g_tbMutex);
    g_tbPath = path;
    g_tbAvailable = false;
    if (!path.empty()) {
        g_tbAvailable = tb_init(path.c_str()) != 0;
    }
    ++g_tbPathUpdates;
    {
        std::lock_guard<std::mutex> cacheLock(g_cacheMutex);
        g_wdlCache.clear();
        g_dtzCache.clear();
    }
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
    if (nikola::countPieces(board) > 7) {
        return 2;
    }
    std::string key = boardToFEN(board);
    {
        std::lock_guard<std::mutex> lock(g_cacheMutex);
        auto it = g_wdlCache.find(key);
        if (it != g_wdlCache.end()) return it->second;
    }
    unsigned res = tbProbeWDL(board);
    int val;
    switch (res) {
        case 4: val = 1; break;   // win
        case 2: val = 0; break;   // draw
        case 0: val = -1; break;  // loss
        default: val = 2; break;  // unknown
    }
    {
        std::lock_guard<std::mutex> lock(g_cacheMutex);
        g_wdlCache[key] = val;
    }
    return val;
}

int probeDTZ(const Board& board) {
    if (!tablebaseAvailable()) {
        return 0;
    }
    if (nikola::countPieces(board) > 7) {
        return 0;
    }
    std::string key = boardToFEN(board);
    {
        std::lock_guard<std::mutex> lock(g_cacheMutex);
        auto it = g_dtzCache.find(key);
        if (it != g_dtzCache.end()) return it->second;
    }
    int val = tbProbeDTZ(board);
    {
        std::lock_guard<std::mutex> lock(g_cacheMutex);
        g_dtzCache[key] = val;
    }
    return val;
}

} // namespace nikola