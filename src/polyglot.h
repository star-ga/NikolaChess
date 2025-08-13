// Polyglot opening book support for Supercomputer Chess Engine.
//
// This header declares functions for loading and probing a Polyglot
// opening book.  Polyglot books are binary files containing
// compact mappings from Zobrist keys to moves, weights and learning
// values.  By enabling the opening book via the OwnBook UCI option
// and specifying a book file via BookFile, the engine can consult
// the book during the opening phase to quickly return strong
// moves without searching.  The API here allows the UCI layer to
// configure and query the book.

#pragma once

#include <optional>
#include <string>
#include "board.h"

namespace nikola {

// Enable or disable the opening book.  When disabled, probeBook
// always returns std::nullopt regardless of whether a book file
// has been loaded.  This function should be called when the
// UCI option OwnBook is processed.
void setUseBook(bool enable);

// Set the path to the Polyglot book file.  When the path is non‑empty,
// the engine will attempt to load the book on first probe.  An
// empty path clears any previously loaded book.  This function
// should be called when the UCI option BookFile is processed.
void setBookFile(const std::string& path);

// Probe the opening book for the given position.  If the book is
// enabled and a matching entry exists, returns a Move representing
// the suggested book move.  If no entry is found or the book is
// disabled or not loaded, returns std::nullopt.  The move
// returned does not include promotion information for non‑promotion
// moves (promotedTo will be zero).  Callers should treat the
// result as a recommended move and skip searching.
std::optional<Move> probeBook(const Board& board);

// Add an entry to the in-memory opening book.  The entry is keyed by the
// current board's Polyglot hash and associates a move with a weight and
// optional learn value.  This is useful for constructing books from PGN
// data at runtime.
void addBookEntry(const Board& board, const Move& move,
                  uint16_t weight = 1, uint16_t learn = 0);

// Write the currently populated in-memory book to the given path in
// standard Polyglot format.  Returns true on success.
bool saveBook(const std::string& path);

} // namespace nikola