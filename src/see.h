// Static Exchange Evaluation (SEE) for Supercomputer Chess Engine.
//
// SEE is a heuristic used to estimate the material balance of a
// potential capture sequence on a given square.  A positive SEE
// value indicates that a capture is likely to be profitable for
// the side to move, while a negative value suggests that the
// sequence loses material.  The implementation here is a very
// simple approximation that considers only the value of the
// immediately captured piece minus the value of the capturing
// piece.  For the purposes of move ordering this is often
// sufficient.  A full SEE implementation would simulate
// subsequent recaptures by both sides and evaluate the net gain.

#pragma once

#include "board.h"

namespace nikola {

// Compute a static exchange evaluation (SEE) score for the given
// capture move on the specified board.  The board should
// represent the position before the move is played.  The move
// must be a capture; if it is not, the function returns zero.
// Positive scores favour the side to move, negative scores favour
// the opponent.  This simple implementation subtracts the value
// of the attacking piece from the value of the captured piece.
int see(const Board& board, const Move& m);

} // namespace nikola