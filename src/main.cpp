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
#include "uci.h"
#include "distributed.h"

#include <iostream>
#include <vector>
#include <string>
#include <exception>
#include <cstdint>

// Optional GPU stream configuration hook.
// If your build provides a real implementation, define
// HAVE_NIKOLA_SET_GPU_STREAMS at compile time and link the provider.
namespace {
#if defined(HAVE_NIKOLA_SET_GPU_STREAMS)
void setGpuStreams(int n);            // provided elsewhere
#else
inline void setGpuStreams(int) {}     // no-op stub
#endif
}

// Forward declaration of search entrypoint (implemented in search.cpp).
// If you have a public header that declares this (e.g. search.h), you can
// include it instead of this forward declaration.
namespace nikola {
Move findBestMove(const Board& board, int depth, int timeLimitMs = 0);
}

// Utility to convert a zero-based square coordinate into algebraic
// notation (e.g. (0,0) -> "a1", (7,7) -> "h8").
static std::string toAlgebraic(int row, int col) {
    char file = static_cast<char>('a' + col);
    char rank = static_cast<char>('1' + row);
    std::string s;
    s += file;
    s += rank;
    return s;
}

int main(int argc, char* argv[]) {
    using namespace nikola;

    // ---- Global option handling (before positional commands) ----
    // Supports:
    //   --gpu-streams <N>   configure number of CUDA streams for GPU evaluator
    //   --distributed       run distributed search mode and exit
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--gpu-streams") {
            if (i + 1 >= argc) {
                std::cerr << "Error: --gpu-streams requires an integer argument.\n";
                return 2;
            }
            int n = 0;
            try {
                n = std::stoi(argv[i + 1]);
            } catch (const std::exception&) {
                std::cerr << "Error: invalid value for --gpu-streams: " << argv[i + 1] << "\n";
                return 2;
            }
            if (n < 0) {
                std::cerr << "Error: --gpu-streams must be non-negative.\n";
                return 2;
            }
            ::setGpuStreams(n);
            // Remove the option and its value from argv/argc (compact in-place)
            for (int j = i; j + 2 <= argc; ++j) {
                argv[j] = argv[j + 2];
            }
            argc -= 2;
            --i; // reprocess current index after compaction
            continue;
        }

        if (arg == "--distributed") {
            // Enter distributed search mode and exit when it returns.
            distributed_search();
            return 0;
        }
    }

    // ---- Positional command interface ----
    // If the first argument is "perft", compute and print the perft value at
    // the specified depth. Example: `./nikolachess perft 4`
    if (argc >= 2) {
        std::string cmd = argv[1];

        // Enter UCI protocol mode when invoked with "uci".
        if (cmd == "uci") {
            runUciLoop();
            return 0;
        }

        if (cmd == "perft") {
            int depth = 1;
            if (argc >= 3) {
                try {
                    depth = std::stoi(argv[2]);
                } catch (const std::exception&) {
                    std::cerr << "Error: invalid depth for perft: " << argv[2] << "\n";
                    return 2;
                }
            }
            Board board = initBoard();
            std::uint64_t nodes = perft(board, depth);
            std::cout << "Perft(" << depth << ") = " << nodes << std::endl;
            return 0;
        }

        // Parse a FEN string from the command line and print its evaluation.
        // Usage: ./nikolachess fen "<fen>"
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
            // Perform a quick search to suggest a move. Use shallow depth/time.
            Move bm = findBestMove(b, 3, 3000);
            std::cout << "Engine selects move: "
                      << toAlgebraic(bm.fromRow, bm.fromCol) << " -> "
                      << toAlgebraic(bm.toRow, bm.toCol) << std::endl;
            return 0;
        }
    }

    // ---- Default demonstration mode ----
    // Initialise the starting position, evaluate on CPU/GPU, and search a move.
    Board board = initBoard();

    // Evaluate using CPU.
    int cpuScore = evaluateBoardCPU(board);
    std::cout << "CPU evaluation of starting position: " << cpuScore << std::endl;

    // Evaluate using GPU.  We wrap the single board in an array of length one.
    std::vector<Board> boards = {board};
    try {
        std::vector<int> gpuScores =
            evaluateBoardsGPU(boards.data(), static_cast<int>(boards.size()));
        if (!gpuScores.empty()) {
            std::cout << "GPU evaluation of starting position: "
                      << gpuScores[0] << std::endl;
        }
    } catch (std::exception& ex) {
        std::cerr << "GPU evaluation failed: " << ex.what() << std::endl;
    }

    // Search up to depth 3 or stop after 3000 milliseconds.
    Move best = findBestMove(board, 3, 3000);
    std::cout << "Engine selects move: "
              << toAlgebraic(best.fromRow, best.fromCol)
              << " -> " << toAlgebraic(best.toRow, best.toCol) << std::endl;

    return 0;
}
