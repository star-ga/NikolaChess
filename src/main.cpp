// NikolaChess entry point.
//
// Copyright (c) 2013 CPUTER Inc.
// All rights reserved.  See the LICENSE file for license terms.
//
// This demonstration program initialises a chess board, prints its
// evaluation using both the CPU and GPU evaluation functions, and
// performs a small minimax search to select a move for the side to
// move.  The resulting move is printed in algebraic coordinates.

#include "board.h"
#include <iostream>
#include <vector>

// Forward declaration of search function defined in search.cpp.
namespace nikola {
// Find the best move using minimax search to the given depth.  When
// timeLimitMs is positive, iterative deepening will stop when the
// allotted time in milliseconds expires.  The default value of
// zero disables the time limit and searches to the specified
// depth.
Move findBestMove(const Board& board, int depth, int timeLimitMs = 0);
}

// Utility to convert a zero‑based square coordinate into algebraic
// notation (e.g. (0,0) -> "a1", (7,7) -> "h8").
static std::string toAlgebraic(int row, int col) {
    char file = 'a' + col;
    char rank = '1' + row;
    std::string s;
    s += file;
    s += rank;
    return s;
}

int main() {
    using namespace nikola;
    // Initialise starting position.
    Board board = initBoard();
    // Evaluate using CPU.
    int cpuScore = evaluateBoardCPU(board);
    std::cout << "CPU evaluation of starting position: " << cpuScore << std::endl;
    // Evaluate using GPU.  We wrap the single board in an array of
    // length one.  In real use cases you would batch many boards.
    std::vector<Board> boards = {board};
    try {
        std::vector<int> gpuScores = evaluateBoardsGPU(boards.data(), (int)boards.size());
        std::cout << "GPU evaluation of starting position: " << gpuScores[0] << std::endl;
    } catch (std::exception& ex) {
        std::cerr << "GPU evaluation failed: " << ex.what() << std::endl;
    }
    // Search for a move.  Depth 2 (one move for each side) keeps the
    // runtime reasonable.  Depth can be increased on powerful GPUs and
    // CPUs, but branching grows exponentially.
    // Search up to depth 3 or stop after 3000 milliseconds.  A deeper
    // search and longer time limit can be chosen on powerful systems.
    Move best = findBestMove(board, 3, 3000);
    std::cout << "Engine selects move: " << toAlgebraic(best.fromRow, best.fromCol)
              << " -> " << toAlgebraic(best.toRow, best.toCol) << std::endl;
    return 0;
}
