// NikolaChess entry point.
//
// Copyright (c) 2025 CPUTER Inc.
// All rights reserved.  See the LICENSE file for license terms.
//
// This demonstration program initialises a chess board, prints its
// evaluation using both the CPU and GPU evaluation functions, and
// performs a small minimax search to select a move for the side to
// move.  The resulting move is printed in algebraic coordinates.

#include "board.h"
#include <iostream>
#include <vector>
#include <string>
#include "uci.h"
#include "distributed.h"

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

int main(int argc, char* argv[]) {
    using namespace nikola;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--distributed") {
            distributed_search();
            return 0;
        }
    }
    // Command‑line interface.  If the first argument is "perft",
    // compute and print the perft value at the specified depth.
    // Example: `./nikolachess perft 4` will output the number of leaf
    // nodes reachable from the starting position in four plies.
    if (argc >= 2) {
        std::string cmd = argv[1];
        // Enter UCI protocol mode when invoked with "uci".  This
        // bypasses the simple demonstration interface and allows the
        // engine to be driven by GUI frontends.  Once the UCI loop
        // returns (on "quit"), exit immediately.
        if (cmd == "uci") {
            runUciLoop();
            return 0;
        }
        if (cmd == "perft") {
            int depth = 1;
            if (argc >= 3) {
                depth = std::stoi(argv[2]);
            }
            Board board = initBoard();
            uint64_t nodes = perft(board, depth);
            std::cout << "Perft(" << depth << ") = " << nodes << std::endl;
            return 0;
        }
        // Parse a FEN string from the command line and print its evaluation
        // and FEN normalised.  Usage: ./nikolachess fen "<fen>"
        if (cmd == "fen" && argc >= 3) {
            std::string fenArg = argv[2];
            // If the FEN contains spaces (as it usually does), join all
            // remaining arguments back together separated by spaces.
            for (int i = 3; i < argc; ++i) {
                fenArg += ' ';
                fenArg += argv[i];
            }
            Board b = parseFEN(fenArg);
            // Evaluate the parsed position on the CPU.
            int score = evaluateBoardCPU(b);
            std::cout << "Parsed FEN: " << fenArg << std::endl;
            std::cout << "CPU evaluation: " << score << std::endl;
            // Print out the normalised FEN from the Board.
            std::string outFEN = boardToFEN(b);
            std::cout << "Normalised FEN: " << outFEN << std::endl;
            // Perform a quick search to suggest a move.  Use shallow depth to
            // avoid long computation.
            Move bm = findBestMove(b, 3, 3000);
            std::cout << "Engine selects move: "
                      << toAlgebraic(bm.fromRow, bm.fromCol) << " -> "
                      << toAlgebraic(bm.toRow, bm.toCol) << std::endl;
            return 0;
        }
    }
    // Otherwise run the default demonstration: initialise the starting
    // position, evaluate it using both CPU and GPU evaluators, and
    // search for a move using iterative deepening.
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
    // Search up to depth 3 or stop after 3000 milliseconds.  A deeper
    // search and longer time limit can be chosen on powerful systems.
    Move best = findBestMove(board, 3, 3000);
    std::cout << "Engine selects move: " << toAlgebraic(best.fromRow, best.fromCol)
              << " -> " << toAlgebraic(best.toRow, best.toCol) << std::endl;
    return 0;
}
