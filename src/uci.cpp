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
#include "search.h"
#include "board.h"
#include "gpu_eval.h" // for possible future integration
#include "tablebase.h" // for setting tablebase path
#include "pgn_logger.h" // for recording moves and saving PGN
#include "san.h"        // for SAN conversion of moves
#include "polyglot.h"    // for opening book support
#include "uci_extensions.h"   // v20

#include <iostream>
#include <sstream>
#include <vector>
#include <cctype>


namespace nikola {
// Global PGN file path used when saving games.  The default can be changed via the UCI option PGNFile.
static std::string g_pgnFilePath = "game.pgn";
// Opening book configuration
static bool g_useBook = false;
static std::string g_bookFilePath;

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
    // Reset PGN logger at the start of the UCI session.  If
    // ucinewgame is received later, the PGN will be reset again.
    resetPgn();
    std::string line;
    while (std::getline(std::cin, line)) {
        // Trim leading and trailing whitespace.
        std::string trimmed = line;
        // Remove CR if present (Windows line endings).
        if (!trimmed.empty() && trimmed.back() == '\r') trimmed.pop_back();

        // Tokenise the entire command for convenience.
        std::vector<std::string> tokens;
        {
            std::istringstream tss(trimmed);
            std::string tok;
            while (tss >> tok) tokens.push_back(tok);
        }
        if (tokens.empty()) continue;

        std::istringstream iss(trimmed);
        std::string cmd;
        if (!(iss >> cmd)) continue;
        if (cmd == "uci") {
            nikola::uci_print_id_and_options();
        } else if (cmd == "isready") {
            nikola::uci_isready();
        } else if (cmd == "ucinewgame") {
            // Reset to starting position and clear any game state.
            board = initBoard();
            // Clear the PGN move list for the new game.
            resetPgn();
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
                            // Compute SAN before applying the move.
                            std::string san = toSAN(board, m);
                            board = makeMove(board, m);
                            // Record the move for PGN logging using SAN.
                            addMoveToPgn(san);
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
                            // Compute SAN on the current board before applying the move.
                            std::string san = toSAN(board, m);
                            board = makeMove(board, m);
                            // Record the move for PGN logging.
                            addMoveToPgn(san);
                        }
                    }
                }
            }
        } else if (cmd == "go") {
            nikola::uci_go(board, tokens);
        } else if (cmd == "stop") {
            // Synchronous search returns immediately; nothing to stop.
            // A real implementation would signal the search thread to halt.
        } else if (cmd == "quit" || cmd == "exit") {
            // Save the PGN to the configured file before exiting.  The
            // file name can be customised via the PGNFile option.
            savePgn(g_pgnFilePath);
            break;
        } else if (cmd == "setoption") {
            nikola::uci_setoption(tokens);
            // Process a UCI setoption command for existing engine options.
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
                } else if (name == "PGNFile") {
                    // Set the PGN output file.  If the provided value is
                    // empty, retain the current file path.  This option
                    // allows the user or GUI to specify where the game
                    // PGN should be saved on quit.
                    if (!value.empty()) {
                        g_pgnFilePath = value;
                    }
                } else if (name == "OwnBook") {
                    // Enable or disable the opening book.  Accept
                    // "true"/"false", "1"/"0", "yes"/"no".  When
                    // disabling the book we do not unload the file.
                    bool use = false;
                    if (!value.empty()) {
                        char c = static_cast<char>(std::tolower(static_cast<unsigned char>(value[0])));
                        use = (c == '1' || c == 't' || c == 'y');
                    }
                    g_useBook = use;
                    nikola::setUseBook(use);
                } else if (name == "BookFile") {
                    // Record the path to the Polyglot book file.  If
                    // value is non‑empty, set g_bookFilePath and
                    // attempt to load the book via polyglot.  If
                    // value is empty, clear the book path but leave
                    // g_useBook unchanged.
                    if (!value.empty()) {
                        g_bookFilePath = value;
                        nikola::setBookFile(value);
                    } else {
                        g_bookFilePath.clear();
                        nikola::setBookFile("");
                    }
                }
                // Other options (Hash, Threads) are currently ignored.
            }
        } else {
            // Unknown command; ignore or log.
        }
    }
}

} // namespace nikola
