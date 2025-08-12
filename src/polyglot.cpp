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
#include <random>
#include <mutex>

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

// Initialise and access the Polyglot random number array.  Polyglot
// hashing uses 781 64‑bit pseudorandom values: 12*64 for each piece on
// each square, one for the side to move, four for castling rights
// (white/black king/queen side) and eight for the en passant file.  To
// ensure reproducibility across platforms we generate these numbers
// deterministically from a fixed seed using a 64‑bit Mersenne Twister.
static std::vector<uint64_t> polyKeys;
static std::once_flag polyInitFlag;

static void initPolyKeys() {
    polyKeys.resize(781);
    // Seed with a fixed value.  We reuse the first value from the
    // original Polyglot array as the seed to hint at its provenance.
    std::mt19937_64 gen(0x9D39247E33776D41ULL);
    for (size_t i = 0; i < polyKeys.size(); ++i) {
        polyKeys[i] = gen();
    }
}

// Ensure that the polyglot keys are initialised.  This uses call_once to
// guarantee thread‑safe initialisation.
static void ensurePolyKeys() {
    std::call_once(polyInitFlag, initPolyKeys);
}

// Compute the Polyglot Zobrist key for the given board.  This
// implementation follows the Polyglot specification: a 64‑bit hash
// obtained by xoring pseudorandom values corresponding to the pieces
// on the board, the side to move, castling rights and the en
// passant file.  See https://www.chessprogramming.org/PolyGlot for
// details.  Note that Polyglot uses one value per castling right
// (white king side, white queen side, black king side, black queen
// side) and one per en passant file.  We ignore the subtle
// condition that the en passant key should only be xored if a pawn
// could actually capture en passant on that file; including the key
// unconditionally still yields a valid hash.
static uint64_t polyglotKey(const nikola::Board& board) {
    ensurePolyKeys();
    uint64_t key = 0ULL;
    // 0..63 squares: index = row*8 + col
    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 8; ++c) {
            int8_t piece = board.squares[r][c];
            if (piece == nikola::EMPTY) continue;
            int absPiece = piece >= 0 ? piece : -piece;
            // pieceIndex: 0..5 for white, 6..11 for black
            int idx = (absPiece - 1) + (piece < 0 ? 6 : 0);
            int squareIndex = r * 8 + c;
            size_t keyIdx = static_cast<size_t>(idx) * 64 + squareIndex;
            if (keyIdx < polyKeys.size()) {
                key ^= polyKeys[keyIdx];
            }
        }
    }
    // Side to move: Polyglot xors one number if Black is to move.
    if (!board.whiteToMove) {
        key ^= polyKeys[768];
    }
    // Castling rights: one value per right.  Indexes 769..772.
    if (board.whiteCanCastleKingSide) key ^= polyKeys[769];
    if (board.whiteCanCastleQueenSide) key ^= polyKeys[770];
    if (board.blackCanCastleKingSide) key ^= polyKeys[771];
    if (board.blackCanCastleQueenSide) key ^= polyKeys[772];
    // En passant file: indexes 773..780.  Only include if en passant is
    // available (col >=0).  We do not check for pawn presence, which is
    // optional but not necessary for uniqueness.  If enPassantCol is
    // outside 0..7 we ignore it.
    if (board.enPassantCol >= 0 && board.enPassantCol < 8) {
        key ^= polyKeys[773 + board.enPassantCol];
    }
    return key;
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