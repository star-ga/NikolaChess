#include "bitboard.h"
#include "board.h"

namespace nikola {

Bitboards boardToBitboards(const Board &board) {
    Bitboards bb{};
    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 8; ++c) {
            int sq = r * 8 + c;
            int8_t p = board.squares[r][c];
            if (p == EMPTY) continue;
            int idx = 0;
            if (p > 0) {
                idx = p - 1; // white pieces 0..5
                bb_set(bb.whiteMask, sq);
            } else {
                idx = (-p - 1) + 6; // black pieces 6..11
                bb_set(bb.blackMask, sq);
            }
            bb_set(bb.pieces[idx], sq);
        }
    }
    bb.occupied = bb.whiteMask | bb.blackMask;
    return bb;
}

} // namespace nikola

