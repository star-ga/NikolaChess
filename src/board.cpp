// Implementation of basic board manipulation functions for NikolaChess.
#include "board.h"

#include <algorithm>

namespace nikola {

Board initBoard() {
    Board b;
    // Fill the board with empty squares.
    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 8; ++c) {
            b.squares[r][c] = EMPTY;
        }
    }
    // Place white pieces.
    b.squares[0][0] = WR; b.squares[0][7] = WR;
    b.squares[0][1] = WN; b.squares[0][6] = WN;
    b.squares[0][2] = WB; b.squares[0][5] = WB;
    b.squares[0][3] = WQ; b.squares[0][4] = WK;
    for (int c = 0; c < 8; ++c) {
        b.squares[1][c] = WP;
    }
    // Place black pieces.
    b.squares[7][0] = BR; b.squares[7][7] = BR;
    b.squares[7][1] = BN; b.squares[7][6] = BN;
    b.squares[7][2] = BB; b.squares[7][5] = BB;
    b.squares[7][3] = BQ; b.squares[7][4] = BK;
    for (int c = 0; c < 8; ++c) {
        b.squares[6][c] = BP;
    }
    b.whiteToMove = true;

    // At the start of the game both sides may castle on both sides.
    b.whiteCanCastleKingSide = true;
    b.whiteCanCastleQueenSide = true;
    b.blackCanCastleKingSide = true;
    b.blackCanCastleQueenSide = true;
    b.enPassantCol = -1;
    return b;
}

// Simple CPU evaluation used to validate GPU results.  It sums
// piece values and applies a small positional bonus for central
// pawn and knight placement.  Real engines employ much more
// sophisticated heuristics.
static int pieceValue(int8_t p) {
    switch (p) {
        case WP: return 100;
        case WN: return 320;
        case WB: return 330;
        case WR: return 500;
        case WQ: return 900;
        case WK: return 100000;
        case BP: return -100;
        case BN: return -320;
        case BB: return -330;
        case BR: return -500;
        case BQ: return -900;
        case BK: return -100000;
        default: return 0;
    }
}

int evaluateBoardCPU(const Board& board) {
    int score = 0;
    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 8; ++c) {
            int8_t p = board.squares[r][c];
            // Sum up material values.
            score += pieceValue(p);
            // Central pawn/knight bonus.  Encourage pawns and knights in
            // the centre of the board.
            if (p == WP && (r == 3 || r == 4) && (c >= 2 && c <= 5)) {
                score += 10;
            } else if (p == BP && (r == 4 || r == 3) && (c >= 2 && c <= 5)) {
                score -= 10;
            } else if ((p == WN || p == BN) && (r >= 2 && r <= 5) && (c >= 2 && c <= 5)) {
                score += (p > 0 ? 30 : -30);
            }
        }
    }
    return score;
}

} // namespace nikola
