// UCI protocol support for NikolaChess.
//
// This header declares functions to run a minimal UCI (Universal
// Chess Interface) loop.  The implementation reads commands from
// standard input and drives the engine accordingly, allowing the
// engine to be used with GUI frontends and tournament managers.  It
// supports the basic subset of the protocol: uci, isready,
// ucinewgame, position, go (with depth or movetime) and quit.  New
// functionality can be added incrementally to implement the full
// specification.

#pragma once

#include <string>

namespace nikola {

// Run the UCI loop.  This function blocks reading commands from
// standard input until a quit command is received.  It prints
// responses to standard output in compliance with the UCI
// specification.
void runUciLoop();

// Accessors for strength limiting options
bool getLimitStrength();
int  getStrength();
void setLimitStrength(bool v);
void setStrength(int v);

} // namespace nikola