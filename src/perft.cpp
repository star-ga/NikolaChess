// Perft (move generation enumeration) implementation for NikolaChess.
//
// This file defines a simple recursive function to count the number
// of leaf nodes reachable from the current position at a given
// depth.  It can be used to validate the move generator by
// comparing the perft values of known positions to published
// references.  See the Chessprogramming Wiki for standard perft
// test suites and expected results.

#include "board.h"

namespace nikola {

// Recursively count the number of leaf nodes reachable within
// `depth` plies.  At depth zero we return 1 to count the current
// position as a leaf.  Otherwise we generate all legal moves,
// recursively call perft on each child position and sum the results.
uint64_t perft(const Board& board, int depth) {
    if (depth <= 0) {
        return 1;
    }
    auto moves = generateMoves(board);
    // If there are no legal moves, this is a leaf node.
    if (moves.empty()) {
        return 1;
    }
    uint64_t nodes = 0;
    for (const Move& m : moves) {
        Board child = makeMove(board, m);
        nodes += perft(child, depth - 1);
    }
    return nodes;
}

} // namespace nikola