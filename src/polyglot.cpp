// Polyglot opening book implementation for Supercomputer Chess Engine.
//
// Copyright (c) 2025 CPUTER Inc.
// All rights reserved.  See LICENSE for terms.

#include "polyglot.h"

#include <cstdint>
#include <fstream>
#include <mutex>
#include <unordered_map>
#include <vector>
#include <string>

namespace {

// Internal state for the opening book.  We store a map from
// Zobrist keys to a vector of moves with associated weights.  The
// actual Zobrist key used in Polyglot books differs from our
// search's Zobrist; computing it requires a separate random table.
// For now we provide a stub implementation that always returns
// zero.  When a real Polyglot key function is implemented, the
// book entries will be keyed properly.

struct BookEntry {
    nikola::Move move;
    uint16_t weight;
    uint16_t learn;
};

// Book data mapped by Polyglot key.
static std::unordered_map<uint64_t, std::vector<BookEntry>> g_book;
// Path to the book file.  Empty means no book loaded.
static std::string g_bookPath;
// Flag indicating whether book usage is enabled.
static bool g_bookEnabled = false;
// Mutex to protect book loading and access.
static std::mutex g_bookMutex;

// Compute the Polyglot Zobrist key for the given board.  Polyglot
// defines its own random keys and includes castling rights and
// en passant file differently from our search key.  A full
// implementation would initialise tables for each piece on each
// square and other state bits.  For now this stub returns zero,
// meaning no positions will ever match the book.  TODO: implement
// proper Polyglot hashing.
static uint64_t polyglotKey(const nikola::Board& /*board*/) {
    return 0ULL;
}

// Load the book from the currently configured g_bookPath into
// g_book.  This function is called lazily on first probe.  If the
// file cannot be opened, the book remains empty.  The Polyglot
// binary format consists of 16‑byte records: 8 bytes key, 2 bytes
// move, 2 bytes weight, 4 bytes learn.  The move encoding uses
// from‑to coordinates and promotion piece per the Polyglot spec.
static void loadBook() {
    std::lock_guard<std::mutex> lock(g_bookMutex);
    // If already loaded or no file path, do nothing.
    if (!g_book.empty() || g_bookPath.empty()) {
        return;
    }
    std::ifstream f(g_bookPath, std::ios::binary);
    if (!f) {
        // Could not open book file.  Leave book empty.
        return;
    }
    // Determine file size to pre‑allocate.
    f.seekg(0, std::ios::end);
    std::streampos fileSize = f.tellg();
    f.seekg(0, std::ios::beg);
    size_t numEntries = static_cast<size_t>(fileSize) / 16;
    g_book.reserve(numEntries);
    for (size_t i = 0; i < numEntries; ++i) {
        uint64_t key;
        uint16_t moveCode;
        uint16_t weight;
        uint32_t learn;
        f.read(reinterpret_cast<char*>(&key), sizeof(key));
        f.read(reinterpret_cast<char*>(&moveCode), sizeof(moveCode));
        f.read(reinterpret_cast<char*>(&weight), sizeof(weight));
        f.read(reinterpret_cast<char*>(&learn), sizeof(learn));
        if (!f) break;
        // Decode the moveCode.  Bits: from (0..63), to (6 bits), prom (2 bits) etc.
        nikola::Move m{};
        int from = (moveCode >> 6) & 0x3F;
        int to = moveCode & 0x3F;
        int prom = (moveCode >> 12) & 0x7; // 3 bits for promotion piece
        m.fromRow = from / 8;
        m.fromCol = from % 8;
        m.toRow = to / 8;
        m.toCol = to % 8;
        m.captured = nikola::EMPTY; // Not recorded in book; will be filled when applied.
        m.promotedTo = 0;
        // Promotion piece encoding: 1=N,2=B,3=R,4=Q.  Promotion pieces are
        // always white in Polyglot; we adjust based on side to move when
        // applying the move in the engine.
        switch (prom) {
            case 1: m.promotedTo = nikola::WN; break;
            case 2: m.promotedTo = nikola::WB; break;
            case 3: m.promotedTo = nikola::WR; break;
            case 4: m.promotedTo = nikola::WQ; break;
            default: m.promotedTo = 0; break;
        }
        // Store the book entry.
        g_book[key].push_back(BookEntry{m, weight, static_cast<uint16_t>(learn)});
    }
}

} // anonymous namespace

namespace nikola {

void setUseBook(bool enable) {
    g_bookEnabled = enable;
}

void setBookFile(const std::string& path) {
    std::lock_guard<std::mutex> lock(g_bookMutex);
    g_bookPath = path;
    g_book.clear();
}

std::optional<Move> probeBook(const Board& board) {
    // If the book is disabled or no file has been provided, return null.
    if (!g_bookEnabled || g_bookPath.empty()) {
        return std::nullopt;
    }
    // Load the book on first use.
    if (g_book.empty()) {
        loadBook();
        // If still empty after loading, give up.
        if (g_book.empty()) {
            return std::nullopt;
        }
    }
    // Compute the Polyglot key for this position.
    uint64_t key = polyglotKey(board);
    auto it = g_book.find(key);
    if (it == g_book.end() || it->second.empty()) {
        return std::nullopt;
    }
    // Choose the entry with the highest weight.  Some books may include
    // multiple entries for the same position.  A more sophisticated
    // selection could use weighted randomness or learning values.
    const auto& vec = it->second;
    const BookEntry* bestEntry = &vec[0];
    for (const auto& e : vec) {
        if (e.weight > bestEntry->weight) {
            bestEntry = &e;
        }
    }
    // Return the move stored in the best entry.  The captured field
    // will be filled by the caller based on the current board state.
    return bestEntry->move;
}

} // namespace nikola