#include "board.h"
#include <cassert>
#include <iostream>

namespace nikola { int tbProbeDTZ(const Board& b); }

extern unsigned last_castling;
extern unsigned last_ep;

int main() {
    using namespace nikola;

    Board b{};
    for (int r = 0; r < 8; ++r)
        for (int c = 0; c < 8; ++c)
            b.squares[r][c] = Piece::EMPTY;
    b.whiteToMove = true;
    b.enPassantCol = -1;
    b.whiteCanCastleKingSide = true;
    int dtz = tbProbeDTZ(b);
    assert(last_castling == 1);
    assert(dtz == 5);

    b.whiteCanCastleKingSide = false;
    dtz = tbProbeDTZ(b);
    assert(last_castling == 0);
    assert(dtz == 0);

    Board ep{};
    for (int r = 0; r < 8; ++r)
        for (int c = 0; c < 8; ++c)
            ep.squares[r][c] = Piece::EMPTY;
    ep.whiteToMove = true;
    ep.enPassantCol = 3;
    dtz = tbProbeDTZ(ep);
    assert(last_ep == 5 * 8 + 3);
    assert(dtz == -5);

    std::cout << "dtzprobe tests passed\n";
    return 0;
}
