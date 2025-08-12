// PGN logger implementation for NikolaChess.
//
// Copyright (c) 2025 CPUTER Inc.
// All rights reserved.  See the LICENSE file for license terms.
//
// This implementation stores moves in a simple vector of strings and
// writes them out in Portable Game Notation when requested.  The
// notation used for moves is the coordinate notation (e.g., "e2e4").
// For a more sophisticated PGN generator one could convert moves to
// Standard Algebraic Notation, detect checks and mates, and record
// additional metadata; this implementation focuses on a minimal
// record of moves played.

#include "pgn_logger.h"

#include <vector>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <filesystem>

namespace nikola {

// Internal storage for moves.  Each element is a move string such
// as "e2e4" or "e7e8q".  The vector is cleared by resetPgn().
static std::vector<std::string> pgnMoves;

void resetPgn() {
    pgnMoves.clear();
}

void addMoveToPgn(const std::string& move) {
    pgnMoves.push_back(move);
}

// Helper to format the move list into a PGN move string.  Moves are
// numbered starting from 1 and grouped as pairs (White and Black).
static std::string formatMoves() {
    std::ostringstream oss;
    int moveNumber = 1;
    for (size_t i = 0; i < pgnMoves.size(); ++i) {
        if (i % 2 == 0) {
            // Start a new move number.
            if (i > 0) oss << ' ';
            oss << moveNumber << ". " << pgnMoves[i];
        } else {
            // Black move follows on same number.
            oss << ' ' << pgnMoves[i];
            moveNumber++;
        }
    }
    return oss.str();
}

void savePgn(const std::string& filePath) {
    // Determine parent directory and create it if it does not exist.
    try {
        std::filesystem::path p(filePath);
        if (p.has_parent_path()) {
            std::filesystem::create_directories(p.parent_path());
        }
    } catch (...) {
        // Ignore directory creation errors; the write will fail below.
    }
    std::ofstream out(filePath);
    if (!out) {
        return;
    }
    // Write minimal PGN headers.  Use current date.
    auto t = std::time(nullptr);
    std::tm tm;
#ifdef _MSC_VER
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    out << "[Event \"Supercomputer Chess Engine Game\"]\n";
    out << "[Site \"Local\"]\n";
    out << "[Date \"" << std::put_time(&tm, "%Y.%m.%d") << "\"]\n";
    out << "[Round \"1\"]\n";
    out << "[White \"Supercomputer\"]\n";
    out << "[Black \"Supercomputer\"]\n";
    out << "[Result \"*\"]\n\n";
    // Write moves.  Break lines every 80 characters for readability.
    std::string movesStr = formatMoves();
    const size_t maxLine = 80;
    size_t pos = 0;
    while (pos < movesStr.size()) {
        size_t len = std::min(maxLine, movesStr.size() - pos);
        // Try to break at a space if possible.
        size_t end = pos + len;
        if (end < movesStr.size() && movesStr[end] != ' ') {
            size_t spacePos = movesStr.rfind(' ', end);
            if (spacePos != std::string::npos && spacePos > pos) {
                end = spacePos;
            }
        }
        out << movesStr.substr(pos, end - pos) << '\n';
        // Skip leading space on next line.
        pos = end;
        while (pos < movesStr.size() && movesStr[pos] == ' ') {
            pos++;
        }
    }
    // Append termination marker.  PGN allows * for unknown result.
    out << " *\n";
}

} // namespace nikola