// Minimal UCI protocol implementation for NikolaChess.
//
// This file implements a subset of the Universal Chess Interface (UCI)
// protocol, enabling the engine to communicate with GUI frontends
// such as CuteChess or Arena.  The implementation is intentionally
// simple and synchronous: it does not support pondering or
// asynchronous search and recognises only a limited set of UCI
// commands.  The supported commands are:
//
//   uci          – Identify the engine and report available options.
//   isready      – Acknowledge readiness (always "readyok").
//   ucinewgame   – Reset the game state to the starting position.
//   position     – Set up a position from FEN or the starting position and apply moves.
//   go           – Start searching; accepts depth and movetime arguments.
//   stop         – No‑op in this implementation (search runs synchronously).
//   quit         – Exit the loop and terminate the engine.
//
// Future work may extend this to support all UCI options, time
// management, ponder hit, multiPV and other features.

#include "uci.h"
#include "board.h"
#include "gpu_eval.h" // for possible future integration
#include "tablebase.h" // for setting tablebase path

#include <iostream>
#include <sstream>
#include <vector>
#include <cctype>

// Forward declaration of search function.
namespace nikola {

// Forward declaration for runtime GPU switching.  Defined in search.cpp.
void setUseGpu(bool use);
Move findBestMove(const Board& board, int depth, int timeLimitMs = 0);
Board makeMove(const Board& board, const Move& m);
}

namespace nikola {

// Helper to convert from row,col to algebraic notation.
static std::string toAlgebraic(int row, int col) {
    char file = static_cast<char>('a' + col);
    char rank = static_cast<char>('1' + row);
    std::string s;
    s += file;
    s += rank;
    return s;
}

// Helper to parse a coordinate string like "e2" into row,col.  If
// parsing fails, returns {-1,-1}.
static std::pair<int,int> parseCoordinate(const std::string& coord) {
    if (coord.size() < 2) return {-1, -1};
    char file = coord[0];
    char rank = coord[1];
    if (file < 'a' || file > 'h' || rank < '1' || rank > '8') return {-1,-1};
    int col = file - 'a';
    int row = rank - '1';
    return {row, col};
}

void runUciLoop() {
    Board board = initBoard();
    std::string line;
    while (std::getline(std::cin, line)) {
        // Trim leading and trailing whitespace.
        std::string trimmed = line;
        // Remove CR if present (Windows line endings).
        if (!trimmed.empty() && trimmed.back() == '\r') trimmed.pop_back();
        std::istringstream iss(trimmed);
        std::string cmd;
        if (!(iss >> cmd)) continue;
        if (cmd == "uci") {
            // Report engine identification and options.
            std::cout << "id name NikolaChess" << std::endl;
            std::cout << "id author Unknown" << std::endl;
            // Declare minimal set of options (hash and threads).  These
            // declarations are placeholders; the current engine does
            // not honour them yet.
            std::cout << "option name Hash type spin default 16 min 1 max 1024" << std::endl;
            std::cout << "option name Threads type spin default 1 min 1 max 32" << std::endl;
            // Option to enable or disable GPU batched evaluation.  When
            // enabled the engine will batch evaluation calls to the GPU
            // instead of using the CPU.  Default is false.
            std::cout << "option name UseGPU type check default false" << std::endl;
            // Option to specify the directory of endgame tablebases.
            // Provide an empty value to disable probing.  A future
            // implementation will verify that the directory contains
            // valid Syzygy or Lomonosov files.  Default is empty.
            std::cout << "option name TablebasePath type string default """ << std::endl;
            std::cout << "uciok" << std::endl;
        } else if (cmd == "isready") {
            std::cout << "readyok" << std::endl;
        } else if (cmd == "ucinewgame") {
            // Reset to starting position and clear any game state.
            board = initBoard();
        } else if (cmd == "position") {
            std::string token;
            if (!(iss >> token)) continue;
            if (token == "startpos") {
                board = initBoard();
                // Check for optional moves after 'startpos'.
                if (iss >> token) {
                    if (token == "moves") {
                        std::string mv;
                        while (iss >> mv) {
                            // Apply each move sequentially.
                            if (mv.length() < 4) continue;
                            auto [fromRow, fromCol] = parseCoordinate(mv.substr(0,2));
                            auto [toRow, toCol] = parseCoordinate(mv.substr(2,2));
                            if (fromRow < 0 || toRow < 0) continue;
                            Move m;
                            m.fromRow = fromRow;
                            m.fromCol = fromCol;
                            m.toRow = toRow;
                            m.toCol = toCol;
                            // Determine if a promotion is specified (5th char).
                            m.promotedTo = 0;
                            if (mv.size() == 5) {
                                char promo = std::tolower(static_cast<unsigned char>(mv[4]));
                                // Map promotion piece letter to piece code based on side to move.
                                if (board.whiteToMove) {
                                    switch (promo) {
                                        case 'q': m.promotedTo = WQ; break;
                                        case 'r': m.promotedTo = WR; break;
                                        case 'b': m.promotedTo = WB; break;
                                        case 'n': m.promotedTo = WN; break;
                                    }
                                } else {
                                    switch (promo) {
                                        case 'q': m.promotedTo = BQ; break;
                                        case 'r': m.promotedTo = BR; break;
                                        case 'b': m.promotedTo = BB; break;
                                        case 'n': m.promotedTo = BN; break;
                                    }
                                }
                            }
                            // Capture the piece currently on the destination square.
                            m.captured = board.squares[m.toRow][m.toCol];
                            board = makeMove(board, m);
                        }
                    }
                }
            } else if (token == "fen") {
                // Read FEN: up to 6 fields.  Collect tokens until we have 6
                // fields or run out.
                std::string fen;
                std::string part;
                int fields = 0;
                while (fields < 6 && iss >> part) {
                    fen += part;
                    ++fields;
                    if (fields < 6) fen += ' ';
                }
                board = parseFEN(fen);
                // Remaining token, if present, may be 'moves'
                if (iss >> part) {
                    if (part == "moves") {
                        std::string mv;
                        while (iss >> mv) {
                            if (mv.length() < 4) continue;
                            auto [fromRow, fromCol] = parseCoordinate(mv.substr(0,2));
                            auto [toRow, toCol] = parseCoordinate(mv.substr(2,2));
                            if (fromRow < 0 || toRow < 0) continue;
                            Move m;
                            m.fromRow = fromRow;
                            m.fromCol = fromCol;
                            m.toRow = toRow;
                            m.toCol = toCol;
                            // Handle promotion
                            m.promotedTo = 0;
                            if (mv.size() == 5) {
                                char promo = std::tolower(static_cast<unsigned char>(mv[4]));
                                if (board.whiteToMove) {
                                    switch (promo) {
                                        case 'q': m.promotedTo = WQ; break;
                                        case 'r': m.promotedTo = WR; break;
                                        case 'b': m.promotedTo = WB; break;
                                        case 'n': m.promotedTo = WN; break;
                                    }
                                } else {
                                    switch (promo) {
                                        case 'q': m.promotedTo = BQ; break;
                                        case 'r': m.promotedTo = BR; break;
                                        case 'b': m.promotedTo = BB; break;
                                        case 'n': m.promotedTo = BN; break;
                                    }
                                }
                            }
                            m.captured = board.squares[m.toRow][m.toCol];
                            board = makeMove(board, m);
                        }
                    }
                }
            }
        } else if (cmd == "go") {
            // Parse parameters for the go command.  Recognised parameters:
            //   depth <n>      – search to fixed depth n.
            //   movetime <ms>  – search for exactly ms milliseconds.
            //   wtime <ms>     – white's remaining time in milliseconds.
            //   btime <ms>     – black's remaining time in milliseconds.
            //   winc <ms>      – white's increment per move in ms.
            //   binc <ms>      – black's increment per move in ms.
            //   movestogo <n>  – number of moves left until the next time control.
            //   infinite       – search indefinitely (treated as very deep).
            // If movetime is specified it takes precedence.  Otherwise
            // if wtime/btime are specified a simple time allocation
            // strategy is used to derive a per‑move time limit.  If
            // none of these parameters are provided a default depth of
            // 4 is searched with no time limit.
            int depth = 0;
            int movetime = 0;
            int wtime = 0, btime = 0;
            int winc = 0, binc = 0;
            int movestogo = 0;
            std::string param;
            while (iss >> param) {
                if (param == "depth") {
                    iss >> depth;
                } else if (param == "movetime") {
                    iss >> movetime;
                } else if (param == "wtime") {
                    iss >> wtime;
                } else if (param == "btime") {
                    iss >> btime;
                } else if (param == "winc") {
                    iss >> winc;
                } else if (param == "binc") {
                    iss >> binc;
                } else if (param == "movestogo") {
                    iss >> movestogo;
                } else if (param == "infinite") {
                    // We'll treat 'infinite' as a very high depth.
                    depth = 64;
                }
            }
            Move best;
            int timeLimitMs = 0;
            if (movetime > 0) {
                timeLimitMs = movetime;
            } else if (wtime > 0 || btime > 0) {
                // Allocate time based on side to move, remaining time and increment.
                bool whiteToMove = board.whiteToMove;
                int remaining = whiteToMove ? wtime : btime;
                int increment = whiteToMove ? winc : binc;
                // If movestogo is zero, assume 30 moves left.  Otherwise use the provided number.
                int movesLeft = movestogo > 0 ? movestogo : 30;
                if (movesLeft <= 0) movesLeft = 30;
                // Simple allocation: spend roughly remaining/movesLeft plus half of increment.
                timeLimitMs = (remaining / movesLeft) + (increment / 2);
                // Clamp to a minimum to ensure at least a small search.
                if (timeLimitMs < 50) timeLimitMs = 50;
            }
            if (timeLimitMs > 0) {
                // Use provided depth if specified; otherwise search deep and stop on time limit.
                int searchDepth = depth > 0 ? depth : 64;
                best = findBestMove(board, searchDepth, timeLimitMs);
            } else if (depth > 0) {
                best = findBestMove(board, depth, 0);
            } else {
                // Default search depth if no parameters given.
                best = findBestMove(board, 4, 0);
            }
            std::string uciMove = toAlgebraic(best.fromRow, best.fromCol) + toAlgebraic(best.toRow, best.toCol);
            // Append promotion letter if needed
            if (best.promotedTo != 0) {
                char promo = 'q';
                switch (best.promotedTo) {
                    case WQ: case BQ: promo = 'q'; break;
                    case WR: case BR: promo = 'r'; break;
                    case WB: case BB: promo = 'b'; break;
                    case WN: case BN: promo = 'n'; break;
                }
                uciMove += promo;
            }
            std::cout << "bestmove " << uciMove << std::endl;
            // Apply the move to the current board as well; UCI engines
            // typically do not modify state on "go" alone, but it
            // simplifies testing via CLI.  Comment out if undesired.
            board = makeMove(board, best);
        } else if (cmd == "stop") {
            // Synchronous search returns immediately; nothing to stop.
            // A real implementation would signal the search thread to halt.
        } else if (cmd == "quit" || cmd == "exit") {
            break;
        } else if (cmd == "setoption") {
            // Process a UCI setoption command.  Format is:
            // setoption name <id> [value <x>]
            std::string token;
            std::string name;
            std::string value;
            bool haveName = false;
            bool haveValue = false;
            while (iss >> token) {
                if (token == "name") {
                    // Read the rest of the tokens until 'value' or end
                    std::ostringstream oss;
                    std::string tmp;
                    while (iss >> tmp && tmp != "value") {
                        if (!name.empty()) oss << ' ';
                        oss << tmp;
                    }
                    name = oss.str();
                    haveName = true;
                    if (tmp == "value") {
                        // Read the value
                        if (iss >> value) {
                            haveValue = true;
                        }
                    }
                    break;
                }
            }
            // Trim whitespace from name and value.
            auto trim = [](const std::string& s) {
                size_t start = s.find_first_not_of(' ');
                size_t end = s.find_last_not_of(' ');
                if (start == std::string::npos) return std::string();
                return s.substr(start, end - start + 1);
            };
            name = trim(name);
            value = trim(value);
            if (haveName) {
                // Handle known options.
                if (name == "UseGPU") {
                    bool use = false;
                    if (!value.empty()) {
                        char c = static_cast<char>(std::tolower(static_cast<unsigned char>(value[0])));
                        use = (c == '1' || c == 't' || c == 'y');
                    }
                    setUseGpu(use);
                } else if (name == "TablebasePath") {
                    // Set the path to the directory containing endgame
                    // tablebases.  When a non‑empty path is provided
                    // the engine will attempt to probe positions with
                    // few pieces against the tablebase.  The actual
                    // probing is implemented in tablebase.cpp; this
                    // stub will simply record that a path has been
                    // provided.
                    nikola::setTablebasePath(value);
                }
                // Other options (Hash, Threads) are currently ignored.
            }
        } else {
            // Unknown command; ignore or log.
        }
    }
}

} // namespace nikola