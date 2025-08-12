// PGN logger for NikolaChess.
//
// This module provides simple facilities to collect moves played
// during a game and write them out in Portable Game Notation (PGN).
// It is intended to be used by the UCI loop: call resetPgn() at the
// start of a new game, addMoveToPgn() after each move, and
// savePgn() when the game ends.  The implementation stores moves
// internally in a vector and formats them into a single PGN game with
// minimal headers.

#pragma once

#include <string>

namespace nikola {

// Reset the PGN move list.  Call this at the start of a new game
// (e.g., upon receiving the ucinewgame command) to clear any
// previously recorded moves.
void resetPgn();

// Append a move in coordinate notation (e.g., "e2e4" or
// "e7e8q") to the PGN move list.  Moves should be added in the
// order they are played.  This function does not attempt to
// convert the coordinate notation to SAN; it simply stores the
// string for later formatting.
void addMoveToPgn(const std::string& move);

// Write the current PGN game to the specified file path.  If
// necessary the directory hierarchy will be created.  The PGN
// includes a minimal set of headers (Event, Site, Date) and then
// the moves formatted with move numbers.  After writing, the
// internal move list is not cleared; call resetPgn() manually if
// beginning a new game.
void savePgn(const std::string& filePath);

} // namespace nikola