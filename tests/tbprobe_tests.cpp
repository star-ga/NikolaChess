#include "board.h"
#include <cassert>
#include <iostream>

namespace nikola {
unsigned tbProbeWDL(const Board& b);
}

extern unsigned last_castling;
extern unsigned last_ep;

static constexpr unsigned TB_WIN = 4;
static constexpr unsigned TB_DRAW = 2;
static constexpr unsigned TB_LOSS = 0;

int main() {
    using namespace nikola;

    // Castling rights affect WDL
    Board b{};
    for (int r = 0; r < 8; ++r)
        for (int c = 0; c < 8; ++c)
            b.squares[r][c] = Piece::EMPTY;
    b.whiteToMove = true;
    b.enPassantCol = -1;
    b.whiteCanCastleKingSide = true;
    b.whiteCanCastleQueenSide = true;
    b.blackCanCastleKingSide = true;
    b.blackCanCastleQueenSide = true;
    unsigned res = tbProbeWDL(b);
    assert(last_castling == 0xF);
    assert(res == TB_WIN);

    b.whiteCanCastleKingSide = false;
    b.whiteCanCastleQueenSide = false;
    b.blackCanCastleKingSide = false;
    b.blackCanCastleQueenSide = false;
    res = tbProbeWDL(b);
    assert(last_castling == 0);
    assert(res == TB_DRAW);

    // En passant affects WDL
    Board epBoard{};
    for (int r = 0; r < 8; ++r)
        for (int c = 0; c < 8; ++c)
            epBoard.squares[r][c] = Piece::EMPTY;
    epBoard.whiteToMove = true;
    epBoard.enPassantCol = 4; // file e
    res = tbProbeWDL(epBoard);
    assert(last_ep == 5 * 8 + 4); // e6
    assert(res == TB_LOSS);

    epBoard.enPassantCol = -1;
    res = tbProbeWDL(epBoard);
    assert(last_ep == 0);
    assert(res == TB_DRAW);

    std::cout << "tbprobe tests passed\n";
    return 0;
}
