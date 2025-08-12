// Standard Algebraic Notation (SAN) conversion for Supercomputer
// Chess Engine.
//
// This header declares a function to convert a move and the
// originating board into a full SAN string.  The conversion
// handles all aspects of SAN required for PGN logging:
//   * Castling (O-O and O-O-O)
//   * Piece letters (N,B,R,Q,K) for non‑pawns
//   * Disambiguation by file and/or rank when multiple
//     identical pieces can reach the destination square
//   * Capture notation ('x') including pawn captures (file followed
//     by 'x')
//   * Destination square (file + rank)
//   * Promotion notation (e.g. e8=Q)
//   * Check '+' and checkmate '#' indicators
// The function applies the move on a copy of the board to
// determine whether it gives check or checkmate by generating
// pseudo‑legal replies and testing legality with the
// isKingInCheck() helper.  It does not indicate stalemate as SAN
// has no specific marker for stalemates.

#pragma once

#include <string>
#include "board.h"

namespace nikola {

// Convert the given move into a SAN string.  The Board provided
// should represent the position before the move is made.  The
// returned string adheres to the standard PGN SAN notation.
// See above for details on handled cases.
std::string toSAN(const Board& board, const Move& m);

} // namespace nikola