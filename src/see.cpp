// Static Exchange Evaluation (SEE) implementation for Supercomputer
// Chess Engine.
//
// Copyright (c) 2025 CPUTER Inc.
// All rights reserved.  See LICENSE for terms.

#include "see.h"

namespace nikola {

// Material values used for SEE.  These mirror the orderingMaterial
// array defined in search.cpp: pawn = 100, knight = 320, bishop = 330,
// rook = 500, queen = 900, king = 100000.  The king value is
// intentionally large so that capturing a king (illegal in chess)
// appears extremely favourable.
static const int pieceValues[6] = {100, 320, 330, 500, 900, 100000};

// Return the material value corresponding to a piece code.  The
// input must be non‑zero.  White and black pieces have the same
// absolute value; the sign is ignored.  Piece codes are in the
// range ±1..±6 as defined in board.h.
static inline int valueOf(int8_t p) {
    int absP = p >= 0 ? p : -p;
    // Pawn=1, Knight=2, Bishop=3, Rook=4, Queen=5, King=6.
    if (absP >= 1 && absP <= 6) {
        return pieceValues[absP - 1];
    }
    return 0;
}

int see(const Board& board, const Move& m) {
    // Only evaluate capture moves; return zero for non‑captures.
    if (m.captured == EMPTY) {
        return 0;
    }
    int capturedVal = valueOf((int8_t)m.captured);
    int attackerVal = valueOf(board.squares[m.fromRow][m.fromCol]);
    // Net gain for the side to move: value of the captured piece
    // minus the value of the attacker.  This does not consider
    // subsequent recaptures and therefore serves as a rough first
    // approximation.  More sophisticated SEE algorithms would
    // simulate the full exchange sequence on the square.
    return capturedVal - attackerVal;
}

} // namespace nikola