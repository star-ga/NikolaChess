// Search algorithms for NikolaChess.
//
// Copyright (c) 2025 CPUTER Inc.
// All rights reserved.  See the LICENSE file for license terms.
//
// This module contains a simple minimax search with alpha‑beta
// pruning.  It operates on copies of the Board structure and
// generates moves using the functions in move_generation.cu.  At leaf
// nodes the static evaluation is obtained by calling the CPU
// evaluation; this could be replaced by GPU evaluation for deeper
// parallel searches.  Because the goal of this demonstration is to
// illustrate how GPU acceleration can be integrated, the search depth
// is kept modest.

#include "board.h"

#include <climits>
#include <chrono>
#include <cstdlib>
#include <mutex>
#include <thread>
#include <atomic>
#include <cstdlib> // for getenv
#include <vector>

// Include tablebase probing functions and piece counting utility.  The
// engine will consult endgame tablebases when the position has
// sufficiently few pieces.  If a tablebase result is available, it
// overrides the heuristic evaluation.
#include "tablebase.h"

// Include the micro-batcher for GPU evaluation.  The batcher
// collects evaluation requests from multiple threads and processes
// them in batches on the GPU.  See micro_batcher.h for details.
#include "micro_batcher.h"

namespace nikola {

// Forward declaration of move generator and evaluation functions.
std::vector<Move> generateMoves(const Board& board);
int evaluateBoardCPU(const Board& board);
std::vector<int> evaluateBoardsGPU(const Board* boards, int nBoards);

// Enumeration of available evaluation backends.  By default the
// engine uses the CPU implementation (NNUE or classical).  If
// NIKOLA_GPU environment variable is set to a non‑zero value, the
// engine will attempt to use the GPU batched evaluation.
enum class EvaluationBackend {
    CPU_NNUE,
    GPU_BATCHED
};

// Determine the evaluation backend at program start based on the
// environment.  If NIKOLA_GPU is defined and not zero, choose
// GPU_BATCHED; otherwise default to CPU_NNUE.
static EvaluationBackend g_evalBackend = []() {
    const char* env = std::getenv("NIKOLA_GPU");
    if (env && std::atoi(env) != 0) {
        return EvaluationBackend::GPU_BATCHED;
    }
    return EvaluationBackend::CPU_NNUE;
}();

// Global pointer to the micro-batcher.  It is initialised on first
// use when GPU_BATCHED backend is selected.  The batcher collects
// boards from calls to staticEvaluate() and performs batched
// evaluation on a worker thread.  When the program exits the
// batcher is destroyed, which signals the worker to stop.
static MicroBatcher* g_batcher = nullptr;

// Helper struct to lazily create and destroy the micro-batcher.
// The constructor reads environment variables NIKOLA_GPU_MAX_BATCH
// and NIKOLA_GPU_MICROBATCH_MS to configure the batch size and
// flush interval.  If these are not set, default values are used.
struct MicroBatcherInitializer {
    MicroBatcherInitializer() {
        if (g_evalBackend == EvaluationBackend::GPU_BATCHED) {
            int maxBatch = 32;
            int flushMs = 2;
            if (const char* mb = std::getenv("NIKOLA_GPU_MAX_BATCH")) {
                int v = std::atoi(mb);
                if (v > 0) maxBatch = v;
            }
            if (const char* fm = std::getenv("NIKOLA_GPU_MICROBATCH_MS")) {
                int v = std::atoi(fm);
                if (v > 0) flushMs = v;
            }
            g_batcher = new MicroBatcher(static_cast<size_t>(maxBatch), flushMs);
        }
    }
    ~MicroBatcherInitializer() {
        if (g_batcher) {
            // Force a final flush and destroy the batcher.
            g_batcher->flush();
            delete g_batcher;
            g_batcher = nullptr;
        }
    }
};

// Static instance ensures the initializer runs before main().
static MicroBatcherInitializer g_batcherInit;

// Allow runtime switching of GPU evaluation.  When called with
// use=true the engine switches to GPU batched evaluation and
// creates a micro-batcher if necessary.  When called with use=false
// the engine reverts to CPU evaluation and destroys any existing
// batcher.  This function is intended to be called from the UCI
// command handler when processing the 'setoption' command for
// UseGPU.
void setUseGpu(bool use) {
    if (use) {
        if (g_evalBackend != EvaluationBackend::GPU_BATCHED) {
            g_evalBackend = EvaluationBackend::GPU_BATCHED;
            // Destroy existing batcher if present.
            if (g_batcher) {
                g_batcher->flush();
                delete g_batcher;
                g_batcher = nullptr;
            }
            // Create a new batcher using environment variables or
            // defaults.  Reuse the logic from the initializer.
            int maxBatch = 32;
            int flushMs = 2;
            if (const char* mb = std::getenv("NIKOLA_GPU_MAX_BATCH")) {
                int v = std::atoi(mb);
                if (v > 0) maxBatch = v;
            }
            if (const char* fm = std::getenv("NIKOLA_GPU_MICROBATCH_MS")) {
                int v = std::atoi(fm);
                if (v > 0) flushMs = v;
            }
            g_batcher = new MicroBatcher(static_cast<size_t>(maxBatch), flushMs);
        }
    } else {
        if (g_evalBackend != EvaluationBackend::CPU_NNUE) {
            g_evalBackend = EvaluationBackend::CPU_NNUE;
            if (g_batcher) {
                g_batcher->flush();
                delete g_batcher;
                g_batcher = nullptr;
            }
        }
    }
}

// Evaluate a board position using the selected backend.  When the
// GPU backend is selected, this function batches a single board and
// calls evaluateBoardsGPU.  If GPU evaluation fails or returns
// empty, falls back to the CPU evaluator.
static int staticEvaluate(const Board& b) {
    // If tablebase probing is enabled and the position is small
    // enough, query the endgame tablebase.  This takes priority
    // over any heuristic evaluation.  A positive return value
    // indicates a forced win for White, zero indicates a draw, and
    // a negative value indicates a forced win for Black.  The
    // absolute magnitude of the returned value is chosen to exceed
    // any heuristic evaluation to ensure the search favours tablebase
    // results.  Unknown results (return value 2) fall through to
    // heuristic evaluation.
    if (nikola::tablebaseAvailable()) {
        int numPieces = nikola::countPieces(b);
        // Lomonosov 7‑man and Syzygy 6‑man tablebases support up to
        // seven pieces including kings.  Since we do not yet have a
        // real probing implementation, the probeWDL function always
        // returns unknown.  Nevertheless this logic is ready for
        // future integration: adjust the threshold as appropriate when
        // adding an actual tablebase engine.
        if (numPieces <= 6) {
            int wdl = nikola::probeWDL(b);
            if (wdl == 1) {
                // White can force a win.
                return 100000;
            } else if (wdl == -1) {
                // Black can force a win.
                return -100000;
            } else if (wdl == 0) {
                // Draw.  Use zero as the score.
                return 0;
            }
            // If wdl == 2 (unknown), fall through.
        }
    }
    // If GPU batched evaluation is selected and a batcher exists,
    // submit the board to the batcher and wait for the result.  This
    // call may block if the batch has not yet been flushed.  Any
    // exceptions thrown by the batcher are caught and we fall back to
    // CPU evaluation.
    if (g_evalBackend == EvaluationBackend::GPU_BATCHED && g_batcher) {
        try {
            std::future<int> fut = g_batcher->submit(b);
            // Wait for the result.  The future will be fulfilled
            // when the batcher processes the batch, which may occur
            // immediately or after a short delay.
            return fut.get();
        } catch (...) {
            // If submitting to the batcher throws (e.g., due to
            // shutdown), fall back to CPU evaluation.
        }
    }
    // Default path: evaluate on the CPU.
    return evaluateBoardCPU(b);
}


// The following code extends the basic minimax implementation with
// detection of draw conditions: threefold repetition, the fifty‑move
// rule and insufficient material.  We implement Zobrist hashing to
// uniquely identify positions (including side to move, castling
// rights and en passant targets) and track the number of times each
// position has occurred along the current search path.  When a
// position appears three times, the function returns a score of zero
// (draw).  The half‑move clock stored in the Board tracks the
// number of half moves since the last capture or pawn move; if this
// reaches 100, the fifty‑move rule applies and the game is drawn.

#include <unordered_map>
#include <cstdint>
#include <random>

namespace {

// Compute a Zobrist hash for the given board.  The hash includes the
// side to move, castling rights, en passant target file and the
// placement of all pieces.  We initialise the random tables on first
// use with a deterministic seed to ensure reproducibility.
static uint64_t computeZobrist(const nikola::Board& board) {
    using nikola::Piece;
    // Static tables of random values.  Index 0..5 for White pieces
    // (pawn..king) and 6..11 for Black pieces.  The 64 squares are
    // numbered 0..63 in rank‑file order.
    static bool init = false;
    static uint64_t pieceTable[12][64];
    static uint64_t castlingTable[4];
    static uint64_t enPassantTable[8];
    static uint64_t sideToMoveKey;
    if (!init) {
        std::mt19937_64 rng(0xABCDEF1234567890ULL);
        std::uniform_int_distribution<uint64_t> dist(0, UINT64_MAX);
        for (int i = 0; i < 12; ++i) {
            for (int sq = 0; sq < 64; ++sq) {
                pieceTable[i][sq] = dist(rng);
            }
        }
        for (int i = 0; i < 4; ++i) {
            castlingTable[i] = dist(rng);
        }
        for (int i = 0; i < 8; ++i) {
            enPassantTable[i] = dist(rng);
        }
        sideToMoveKey = dist(rng);
        init = true;
    }
    uint64_t h = 0;
    // Piece placement
    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 8; ++c) {
            int8_t p = board.squares[r][c];
            if (p == nikola::EMPTY) continue;
            int idx;
            if (p > 0) {
                idx = (int)p - 1; // 0..5 for white
            } else {
                idx = (-p) - 1 + 6; // 6..11 for black
            }
            int sq = r * 8 + c;
            h ^= pieceTable[idx][sq];
        }
    }
    // Castling rights
    if (board.whiteCanCastleKingSide) h ^= castlingTable[0];
    if (board.whiteCanCastleQueenSide) h ^= castlingTable[1];
    if (board.blackCanCastleKingSide) h ^= castlingTable[2];
    if (board.blackCanCastleQueenSide) h ^= castlingTable[3];
    // En passant file
    if (board.enPassantCol >= 0 && board.enPassantCol < 8) {
        h ^= enPassantTable[board.enPassantCol];
    }
    // Side to move: we flip the key when White is to move to
    // distinguish positions with the same piece placement but
    // different players to move.
    if (board.whiteToMove) h ^= sideToMoveKey;
    return h;
}

// Determine whether a position has insufficient material to force a
// checkmate.  Returns true if both sides lack sufficient force to
// deliver mate according to simplified heuristics.  We treat the
// following scenarios as draws: (1) king vs king; (2) king and
// single bishop or single knight vs king; (3) king and bishop vs
// king and bishop with both bishops on the same colour; (4) king and
// single knight vs king and single knight.  Positions with pawns,
// rooks or queens are never considered insufficient.  Positions with
// more than one minor piece (bishop or knight) on a side are
// considered sufficient.
static bool isInsufficientMaterial(const nikola::Board& board) {
    int whitePawns = 0, blackPawns = 0;
    int whiteRooks = 0, blackRooks = 0;
    int whiteQueens = 0, blackQueens = 0;
    int whiteKnights = 0, blackKnights = 0;
    int whiteBishops = 0, blackBishops = 0;
    // Track bishop colours for each side; bit 0 = light, bit 1 = dark.
    int whiteBishopColors = 0;
    int blackBishopColors = 0;
    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 8; ++c) {
            int8_t p = board.squares[r][c];
            if (p == nikola::EMPTY) continue;
            bool isWhite = (p > 0);
            switch (p) {
                case nikola::WP: whitePawns++; break;
                case nikola::BP: blackPawns++; break;
                case nikola::WR: whiteRooks++; break;
                case nikola::BR: blackRooks++; break;
                case nikola::WQ: whiteQueens++; break;
                case nikola::BQ: blackQueens++; break;
                case nikola::WN: whiteKnights++; break;
                case nikola::BN: blackKnights++; break;
                case nikola::WB: {
                    whiteBishops++;
                    int color = ((r + c) % 2); // 0 for dark, 1 for light
                    whiteBishopColors |= (1 << color);
                    break;
                }
                case nikola::BB: {
                    blackBishops++;
                    int color = ((r + c) % 2);
                    blackBishopColors |= (1 << color);
                    break;
                }
                default:
                    break;
            }
        }
    }
    // Any pawns, rooks or queens means sufficient material.
    if (whitePawns + blackPawns > 0) return false;
    if (whiteRooks + blackRooks > 0) return false;
    if (whiteQueens + blackQueens > 0) return false;
    // Count minor pieces.
    int whiteMinor = whiteBishops + whiteKnights;
    int blackMinor = blackBishops + blackKnights;
    // Both sides with no minor pieces: king vs king.
    if (whiteMinor == 0 && blackMinor == 0) return true;
    // One side has a single minor, other has none: K+B vs K or K+N vs K.
    if ((whiteMinor == 1 && blackMinor == 0) || (whiteMinor == 0 && blackMinor == 1)) {
        return true;
    }
    // Both sides with exactly one bishop and no knights: K+B vs K+B with bishops on same colour.
    if (whiteBishops == 1 && blackBishops == 1 && whiteKnights == 0 && blackKnights == 0) {
        // Both bishops must be on the same colour squares.
        // If both have both colours, it cannot be insufficient.
        if (whiteBishopColors == 1 && blackBishopColors == 1) return true;
        if (whiteBishopColors == 2 && blackBishopColors == 2) return true;
        // If one has dark and the other has dark, or both have light.
        if (whiteBishopColors == blackBishopColors) return true;
    }
    // Both sides with a single knight: K+N vs K+N.
    if (whiteKnights == 1 && blackKnights == 1 && whiteBishops == 0 && blackBishops == 0) {
        return true;
    }
    // Two knights vs bare king is insufficient to force mate.  If one side
    // has exactly two knights and the other has no minor pieces, it is a draw.
    if (whiteKnights == 2 && whiteBishops == 0 && whitePawns + whiteRooks + whiteQueens == 0 &&
        blackKnights == 0 && blackBishops == 0 && blackPawns + blackRooks + blackQueens == 0) {
        return true;
    }
    if (blackKnights == 2 && blackBishops == 0 && blackPawns + blackRooks + blackQueens == 0 &&
        whiteKnights == 0 && whiteBishops == 0 && whitePawns + whiteRooks + whiteQueens == 0) {
        return true;
    }
    // All other cases: assume sufficient material to continue.
    return false;
}

// Transposition table entry.  Stores the depth at which a position
// was evaluated, the evaluation score, a bound flag and the best
// move found from this position.  The flag takes one of three
// values: EXACT indicates the stored score is the precise evaluation;
// LOWERBOUND means the score is a lower bound (alpha cutoff);
// UPPERBOUND means the score is an upper bound (beta cutoff).
struct TTEntry {
    int depth;
    int score;
    int flag;
    nikola::Move bestMove;
};
static const int TT_EXACT = 0;
static const int TT_LOWERBOUND = 1;
static const int TT_UPPERBOUND = 2;
static std::unordered_map<uint64_t, TTEntry> transTable;

// Mutex to protect access to the transposition table across
// multiple threads.  The transposition table is shared among all
// search threads so that work from one thread benefits others.  A
// coarse‑grained lock is used for simplicity.  For high
// performance engines, a finer‑grained or lock‑free table would
// be preferable.
static std::mutex ttMutex;

// Piece material values used for move ordering.  These mirror the
// values used in the evaluation function: pawn = 100, knight = 320,
// bishop = 330, rook = 500, queen = 900, king = 100000.  We use
// these values to prioritise captures and promotions during search.
static const int orderingMaterial[6] = {100, 320, 330, 500, 900, 100000};

// Killer move heuristic: for each ply store the two moves that most
// recently caused a beta cutoff.  Moves are promoted to the front
// of the move ordering at the corresponding depth.  The indices
// correspond to the ply from the root (0 at root).  We support
// depths up to 64 plies.
// Use thread_local storage for killer moves so that each thread
// maintains its own history without data races.  Each thread
// initialises its killer move table independently.  Depth up to 64 plies.
thread_local static Move killerMoves[64][2];

// History heuristic: a table accumulating a score for each move
// (from square × to square).  Moves that frequently cause beta
// cutoffs at deeper depths receive higher scores and are tried
// earlier.  The table index is [fromSquare][toSquare] where
// fromSquare = fromRow * 8 + fromCol and toSquare = toRow * 8 + toCol.
// History heuristic table is also thread‑local.  Each thread
// accumulates its own history scores for move ordering.  The
// index [from][to] corresponds to moves from a square to a square.
thread_local static int historyTable[64][64];

// Helper to obtain the material value of a piece code.  Input must
// be non‑zero.  The absolute piece code minus one indexes into the
// orderingMaterial array.  White and black pieces have the same
// absolute value.
static inline int orderingPieceValue(int8_t p) {
    int idx = std::abs(p) - 1;
    return orderingMaterial[idx];
}

// Extended minimax with alpha‑beta pruning and draw detection.  The
// repetitions map counts occurrences of positions along the current
// search path.  When a position occurs three times, the game is
// drawn and zero is returned.  The half‑move clock is checked for
// the fifty‑move rule.  Insufficient material also yields a draw.
static int minimax(const nikola::Board& board, int depth, int ply, int alpha, int beta, bool maximizing,
                   std::unordered_map<uint64_t,int>& repetitions) {
    // Fifty-move rule: if 100 half moves have occurred without a
    // capture, pawn move or promotion, return a draw score.
    if (board.halfMoveClock >= 100) {
        return 0;
    }
    // Insufficient material: immediate draw.
    if (isInsufficientMaterial(board)) {
        return 0;
    }
    // Compute the Zobrist hash once for both repetition detection and
    // transposition table lookup.
    uint64_t h = computeZobrist(board);
    // Threefold repetition detection.  Update count for the current
    // position.  If this is the third occurrence, return a draw.
    int& cnt = repetitions[h];
    cnt++;
    if (cnt >= 3) {
        cnt--;
        return 0;
    }
    // Check the transposition table for a cached result at sufficient
    // depth.  If found, use the stored bounds to narrow the window or
    // return an exact evaluation.  Note: transposition table entries
    // do not account for repetition counts or 50‑move rule, so draw
    // detection happens before TT lookup.
    // Lookup in the transposition table.  We copy the entry out of the table
    // while holding the mutex so that other threads can still access the
    // table concurrently.  After copying we release the lock and use
    // the local copy.  This avoids holding the lock during the rest
    // of the search.
    TTEntry ttEntry;
    bool ttFound = false;
    {
        std::lock_guard<std::mutex> lock(ttMutex);
        auto it = transTable.find(h);
        if (it != transTable.end()) {
            ttEntry = it->second;
            ttFound = true;
        }
    }
    if (ttFound && ttEntry.depth >= depth) {
        if (ttEntry.flag == TT_EXACT) {
            cnt--;
            return ttEntry.score;
        } else if (ttEntry.flag == TT_LOWERBOUND) {
            if (ttEntry.score > alpha) alpha = ttEntry.score;
        } else if (ttEntry.flag == TT_UPPERBOUND) {
            if (ttEntry.score < beta) beta = ttEntry.score;
        }
        if (alpha >= beta) {
            cnt--;
            return ttEntry.score;
        }

    // Null‑move pruning.  If the side to move is not in check and
    // sufficient depth remains, try a null move (skip the turn) to
    // quickly detect quiet positions where the opponent cannot take
    // advantage.  We avoid null moves in situations with very few
    // pieces to reduce the risk of zugzwang.  A reduction R of 2 or
    // 3 plies is applied based on remaining depth.  If the null
    // move causes a fail‑high (maximizing) or fail‑low (minimizing)
    // condition, the branch is pruned.
    if (depth >= 2) {
        // Only attempt a null move if the side to move is not in check.
        bool inCheck = isKingInCheck(board, board.whiteToMove);
        if (!inCheck) {
            // Require more than three pieces on the board to avoid
            // zugzwang in simple endgames.
            int numPieces = nikola::countPieces(board);
            if (numPieces > 3) {
                // Determine the reduction.  Use a larger reduction for
                // deeper nodes to make null‑move pruning more
                // aggressive.
                int R = (depth > 6 ? 3 : 2);
                if (depth - 1 - R >= 0) {
                    Board nullBoard = board;
                    // Switch side to move and clear en passant target.
                    nullBoard.whiteToMove = !board.whiteToMove;
                    nullBoard.enPassantCol = -1;
                    // Increment halfmove clock since no capture or pawn
                    // move occurred.
                    nullBoard.halfMoveClock = board.halfMoveClock + 1;
                    // Copy the repetition map so the null move does not
                    // affect other branches.  The minimax call will
                    // increment/decrement repetition counts
                    // independently.
                    std::unordered_map<uint64_t,int> repsNull = repetitions;
                    int nullScore = minimax(nullBoard, depth - 1 - R, ply + 1, alpha, beta,
                                            !maximizing, repsNull);
                    // If the null move evaluation triggers a cutoff, prune.
                    if (maximizing) {
                        // For the maximizing side, a score >= beta is a fail‑high.
                        if (nullScore >= beta) {
                            cnt--;
                            return nullScore;
                        }
                    } else {
                        // For the minimizing side, a score <= alpha is a fail‑low.
                        if (nullScore <= alpha) {
                            cnt--;
                            return nullScore;
                        }
                    }
                }
            }
        }
    }
    }
    // If depth is zero or no moves, perform static evaluation.  We do
    // not consult the TT again here because shallow positions are
    // inexpensive to evaluate and we still need to adjust the
    // repetition count.
    if (depth == 0) {
        int val = staticEvaluate(board);
        cnt--;
        return val;
    }
    auto moves = generateMoves(board);
    if (moves.empty()) {
        int val = staticEvaluate(board);
        cnt--;
        return val;
    }
    // Determine principal variation (PV) move from the TT if available
    // to improve move ordering.  This move should be searched first.
    Move pvMove{};
    bool hasPV = false;
    // Retrieve the principal variation move from the transposition table.
    {
        std::lock_guard<std::mutex> lock(ttMutex);
        auto itPV = transTable.find(h);
        if (itPV != transTable.end()) {
            pvMove = itPV->second.bestMove;
        }
    }
    // Only consider PV if it has a non‑zero from square (valid move).
    if (!(pvMove.fromRow == 0 && pvMove.fromCol == 0 && pvMove.toRow == 0 && pvMove.toCol == 0)) {
        hasPV = true;
    }
    // Assign ordering scores to moves.  Moves that capture a high
    // valued piece or promote are ordered first.  The principal
    // variation move receives the highest priority.
    std::vector<std::pair<int, Move>> scored;
    scored.reserve(moves.size());
    for (const Move& m : moves) {
        int score = 0;
        // PV move bonus
        if (hasPV && m.fromRow == pvMove.fromRow && m.fromCol == pvMove.fromCol &&
            m.toRow == pvMove.toRow && m.toCol == pvMove.toCol && m.promotedTo == pvMove.promotedTo) {
            score += 1000000;
        }
        // Promotion bonus
        if (m.promotedTo != 0) {
            // Promotions are highly valuable.  Use the value of the
            // promoted piece (minus pawn) to scale.
            int idx = std::abs(m.promotedTo) - 1;
            score += 10000 + orderingMaterial[idx];
        }
        // Capture bonus using MVV‑LVA heuristic: prioritize captures
        // of high valued pieces with low valued attackers.
        if (m.captured != nikola::EMPTY) {
            int capturedVal = orderingPieceValue((int8_t)m.captured);
            int attackerVal = orderingPieceValue(board.squares[m.fromRow][m.fromCol]);
            score += 10 * capturedVal - attackerVal;
        }
        // Killer move bonus: moves that previously caused a beta cutoff
        // at this ply are tried earlier.  First killer receives a
        // large bonus; second killer a slightly smaller bonus.
        if (ply < 64) {
            const Move& k0 = killerMoves[ply][0];
            if (m.fromRow == k0.fromRow && m.fromCol == k0.fromCol &&
                m.toRow == k0.toRow && m.toCol == k0.toCol && m.promotedTo == k0.promotedTo) {
                score += 900000;
            } else {
                const Move& k1 = killerMoves[ply][1];
                if (m.fromRow == k1.fromRow && m.fromCol == k1.fromCol &&
                    m.toRow == k1.toRow && m.toCol == k1.toCol && m.promotedTo == k1.promotedTo) {
                    score += 800000;
                }
            }
        }
        // History heuristic: accumulate scores based on past beta cutoffs.
        int fromIdx = m.fromRow * 8 + m.fromCol;
        int toIdx = m.toRow * 8 + m.toCol;
        score += historyTable[fromIdx][toIdx];
        scored.emplace_back(score, m);
    }
    // Sort moves by descending score to improve pruning.  A stable
    // sort ensures the relative order of equally scored moves
    // remains deterministic.
    std::stable_sort(scored.begin(), scored.end(), [](const auto& a, const auto& b) {
        return a.first > b.first;
    });
    int bestVal;
    Move bestChildMove{};
    // Save original alpha and beta to determine the TT flag later.
    int alphaOrig = alpha;
    int betaOrig = beta;
    if (maximizing) {
        bestVal = INT_MIN;
        // Iterate with index to apply late move reductions (LMR).
        for (size_t idx = 0; idx < scored.size(); ++idx) {
            const Move& m = scored[idx].second;
            Board child = makeMove(board, m);
            int nextDepth = depth - 1;
            int eval;
            // Apply late move reductions on quiet moves.  If this is
            // not one of the first few moves and the move is
            // neither a capture nor a promotion, search at reduced
            // depth.  If the reduced search fails to improve alpha
            // we accept the reduced result; otherwise we re‑search
            // at full depth.  The reduction value increases with
            // depth and position in the move list.
            bool isQuiet = (m.captured == nikola::EMPTY && m.promotedTo == 0);
            if (nextDepth > 0 && isQuiet && idx >= 3 && depth >= 3) {
                int reduction = 1;
                if (depth > 6 && idx >= 6) {
                    // Apply a larger reduction deeper in the tree
                    reduction = 2;
                }
                int reducedDepth = nextDepth - reduction;
                if (reducedDepth < 0) reducedDepth = 0;
                int evalReduced = minimax(child, reducedDepth, ply + 1, alpha, beta, false, repetitions);
                eval = evalReduced;
                // If the reduced search suggests the move is good, re‑search
                // at full depth to confirm.
                if (evalReduced > alpha) {
                    eval = minimax(child, nextDepth, ply + 1, alpha, beta, false, repetitions);
                }
            } else {
                eval = minimax(child, nextDepth, ply + 1, alpha, beta, false, repetitions);
            }
            if (eval > bestVal) {
                bestVal = eval;
                bestChildMove = m;
            }
            if (eval > alpha) alpha = eval;
            if (beta <= alpha) {
                // Beta cutoff: record killer move and update history.
                if (ply < 64) {
                    // Promote to first killer if different.
                    if (!(killerMoves[ply][0].fromRow == m.fromRow && killerMoves[ply][0].fromCol == m.fromCol &&
                          killerMoves[ply][0].toRow == m.toRow && killerMoves[ply][0].toCol == m.toCol &&
                          killerMoves[ply][0].promotedTo == m.promotedTo)) {
                        killerMoves[ply][1] = killerMoves[ply][0];
                        killerMoves[ply][0] = m;
                    }
                }
                int fIdx = m.fromRow * 8 + m.fromCol;
                int tIdx = m.toRow * 8 + m.toCol;
                historyTable[fIdx][tIdx] += depth * depth;
                break; // beta cutoff
            }
        }
    } else {
        bestVal = INT_MAX;
        for (size_t idx = 0; idx < scored.size(); ++idx) {
            const Move& m = scored[idx].second;
            Board child = makeMove(board, m);
            int nextDepth = depth - 1;
            int eval;
            bool isQuiet = (m.captured == nikola::EMPTY && m.promotedTo == 0);
            if (nextDepth > 0 && isQuiet && idx >= 3 && depth >= 3) {
                int reduction = 1;
                if (depth > 6 && idx >= 6) {
                    reduction = 2;
                }
                int reducedDepth = nextDepth - reduction;
                if (reducedDepth < 0) reducedDepth = 0;
                int evalReduced = minimax(child, reducedDepth, ply + 1, alpha, beta, true, repetitions);
                eval = evalReduced;
                if (evalReduced < beta) {
                    eval = minimax(child, nextDepth, ply + 1, alpha, beta, true, repetitions);
                }
            } else {
                eval = minimax(child, nextDepth, ply + 1, alpha, beta, true, repetitions);
            }
            if (eval < bestVal) {
                bestVal = eval;
                bestChildMove = m;
            }
            if (eval < beta) beta = eval;
            if (beta <= alpha) {
                // Alpha cutoff: record killer move and update history.
                if (ply < 64) {
                    if (!(killerMoves[ply][0].fromRow == m.fromRow && killerMoves[ply][0].fromCol == m.fromCol &&
                          killerMoves[ply][0].toRow == m.toRow && killerMoves[ply][0].toCol == m.toCol &&
                          killerMoves[ply][0].promotedTo == m.promotedTo)) {
                        killerMoves[ply][1] = killerMoves[ply][0];
                        killerMoves[ply][0] = m;
                    }
                }
                int fIdx = m.fromRow * 8 + m.fromCol;
                int tIdx = m.toRow * 8 + m.toCol;
                historyTable[fIdx][tIdx] += depth * depth;
                break;
            }
        }
    }
    // Store the result in the transposition table.  Only store
    // positions evaluated at this depth or deeper to avoid
    // overwriting more accurate information.  Determine the flag
    // based on the relation of bestVal to the original alpha/beta
    // window.
    int flag;
    if (bestVal <= alphaOrig) {
        flag = TT_UPPERBOUND;
    } else if (bestVal >= betaOrig) {
        flag = TT_LOWERBOUND;
    } else {
        flag = TT_EXACT;
    }
    TTEntry newEntry{depth, bestVal, flag, bestChildMove};
    {
        std::lock_guard<std::mutex> lock(ttMutex);
        auto existing = transTable.find(h);
        if (existing == transTable.end() || existing->second.depth < depth) {
            transTable[h] = newEntry;
        }
    }
    // Restore repetition count when backtracking.
    cnt--;
    return bestVal;
}

// Choose the best move for the current player using minimax search to a
// given depth.  Returns the move that yields the highest (for White)
// or lowest (for Black) evaluation.  If there are no legal moves,
// returns a zeroed move.
Move findBestMove(const Board& board, int depth, int timeLimitMs) {
    Move chosenMove{};
    // Clear the transposition table to avoid interference from
    // previous searches.  We retain the table across iterative
    // deepening depths to allow re‑use of work from shallower
    // searches but clear it on each new top‑level search.
    {
        // Clear the transposition table under lock to avoid races
        std::lock_guard<std::mutex> lock(ttMutex);
        transTable.clear();
    }
    // Reset killer moves and history heuristic tables at the start of
    // a new search.  This ensures that ordering heuristics reflect
    // only information gathered during the current search.
    for (int i = 0; i < 64; ++i) {
        for (int k = 0; k < 2; ++k) {
            killerMoves[i][k] = Move{};
        }
        for (int j = 0; j < 64; ++j) {
            historyTable[i][j] = 0;
        }
    }
    // If no moves are available, return an empty move immediately.
    auto initialMoves = generateMoves(board);
    if (initialMoves.empty()) return chosenMove;
    // If no explicit time limit is provided (zero or negative), derive
    // a reasonable per‑move allocation from environment variables.  A
    // typical strategy is to spend a small fraction of the remaining
    // time plus half of the increment on the current move.  The
    // environment variables `NIKOLA_REMAINING_MS` and
    // `NIKOLA_INCREMENT_MS` can be set by the host program or GUI to
    // provide the remaining time (in milliseconds) and per‑move
    // increment.  If neither is set, a default of 3000 ms is used.
    if (timeLimitMs <= 0) {
        int defaultMs = 3000;
        const char* envRem = std::getenv("NIKOLA_REMAINING_MS");
        const char* envInc = std::getenv("NIKOLA_INCREMENT_MS");
        int remaining = envRem ? std::atoi(envRem) : 0;
        int increment = envInc ? std::atoi(envInc) : 0;
        if (remaining > 0) {
            // Allocate roughly one‑thirtieth of the remaining time and
            // half of the increment.  Clamp to a minimum of 50 ms.
            int alloc = remaining / 30 + increment / 2;
            if (alloc < 50) alloc = 50;
            timeLimitMs = alloc;
        } else {
            timeLimitMs = defaultMs;
        }
    }
    // Track the start time when a time limit is provided.  We use
    // steady_clock to avoid issues with system clock adjustments.
    using clock = std::chrono::steady_clock;
    auto start = clock::now();

    // Determine the number of search threads from the environment.  If
    // NIKOLA_THREADS is set to a value greater than one, the root
    // moves will be searched in parallel using that many threads.
    int numThreads = 1;
    if (const char* envThr = std::getenv("NIKOLA_THREADS")) {
        int t = std::atoi(envThr);
        if (t > 1) numThreads = t;
    }
    // Iterative deepening: search incrementally deeper and keep track
    // of the best move at each depth.  The principal variation
    // information stored in the transposition table will guide move
    // ordering in deeper searches.  If a time limit is set and
    // exceeded, we stop searching and return the best move found
    // thus far.
    // Previous best score used to centre the aspiration window.  We
    // initialise with zero at the root.  The aspiration window is
    // centred around this score to restrict the alpha‑beta window and
    // accelerate search.  On a fail high or fail low we research
    // with the full window.
    int prevBestScore = 0;
    int aspirationWindow = 50;
    for (int currentDepth = 1; currentDepth <= depth; ++currentDepth) {
        Move bestMoveAtDepth{};
        int bestScoreAtDepth = board.whiteToMove ? INT_MIN : INT_MAX;
        // Re‑generate root moves at each depth because the transposition
        // table may have learned a new principal variation.  Use
        // simple ordering: search the PV move first if available.
        auto moves = generateMoves(board);
        // Try to move the PV move (if any) to the front of the list.
        uint64_t rootHash = computeZobrist(board);
        Move pv{};
        bool pvExists = false;
        {
            std::lock_guard<std::mutex> lock(ttMutex);
            auto itPV = transTable.find(rootHash);
            if (itPV != transTable.end()) {
                pv = itPV->second.bestMove;
            }
        }
        if (!(pv.fromRow == 0 && pv.fromCol == 0 && pv.toRow == 0 && pv.toCol == 0)) {
            pvExists = true;
        }
        if (pvExists) {
            // Find pv in moves and move it to the front.
            for (size_t i = 0; i < moves.size(); ++i) {
                const Move& m = moves[i];
                if (m.fromRow == pv.fromRow && m.fromCol == pv.fromCol &&
                    m.toRow == pv.toRow && m.toCol == pv.toCol && m.promotedTo == pv.promotedTo) {
                    if (i != 0) {
                        std::swap(moves[0], moves[i]);
                    }
                    break;
                }
            }
        }
        // Evaluate each move at the current depth.  If multiple
        // threads are requested, distribute the root moves across
        // threads and search them in parallel.  Otherwise perform
        // sequential search with an aspiration window centred on
        // prevBestScore.  The parallel path uses full alpha‑beta
        // bounds for simplicity.
        if (numThreads > 1 && moves.size() > 1) {
            // Shared index into the moves vector.  Each thread will
            // atomically fetch and increment this index to claim the
            // next move to evaluate.
            std::atomic<int> nextIndex(0);
            // Flag to indicate the time limit has been reached.  When
            // set, threads will exit early.
            std::atomic<bool> timeUp(false);
            // Protect updates to the best move and score found so far.
            std::mutex bestMutex;
            Move bestLocalMove{};
            int bestLocalScore = board.whiteToMove ? INT_MIN : INT_MAX;
            // Lambda for worker threads.
            auto worker = [&]() {
                // Each thread resets its own killer and history tables
                // before starting the search to avoid interference from
                // previous searches.  The thread_local variables ensure
                // isolation across threads.
                for (int i = 0; i < 64; ++i) {
                    for (int k = 0; k < 2; ++k) {
                        killerMoves[i][k] = Move{};
                    }
                    for (int j = 0; j < 64; ++j) {
                        historyTable[i][j] = 0;
                    }
                }
                
                while (true) {
                    // Check global time limit.  If expired, signal other
                    // threads to stop and exit.
                    if (timeLimitMs > 0) {
                        auto now = clock::now();
                        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
                        if (elapsed >= timeLimitMs) {
                            timeUp.store(true);
                            break;
                        }
                    }
                    // Fetch the next move index.
                    int idx = nextIndex.fetch_add(1);
                    if (idx >= (int)moves.size() || timeUp.load()) {
                        break;
                    }
                    const Move& m = moves[idx];
                    Board child = makeMove(board, m);
                    std::unordered_map<uint64_t,int> reps;
                    reps[rootHash] = 1;
                    // Search the child position at depth‑1 using full
                    // alpha‑beta bounds.  We do not use aspiration
                    // windows here to avoid coordination overhead.
                    int score = minimax(child, currentDepth - 1, 1, INT_MIN, INT_MAX,
                                        !board.whiteToMove, reps);
                    // Update the best move/score found so far.  Use
                    // locks to protect shared state.
                    {
                        std::lock_guard<std::mutex> guard(bestMutex);
                        if (board.whiteToMove) {
                            if (score > bestLocalScore) {
                                bestLocalScore = score;
                                bestLocalMove = m;
                            }
                        } else {
                            if (score < bestLocalScore) {
                                bestLocalScore = score;
                                bestLocalMove = m;
                            }
                        }
                    }
                }
            };
            // Launch threads.  We spawn numThreads threads to
            // evaluate the root moves.  The current thread also
            // participates in evaluation by running the worker.
            std::vector<std::thread> threads;
            threads.reserve(numThreads - 1);
            for (int t = 1; t < numThreads; ++t) {
                threads.emplace_back(worker);
            }
            // Run worker in the current thread.
            worker();
            // Join all spawned threads.
            for (auto& th : threads) {
                if (th.joinable()) th.join();
            }
            // Use the best move/score found by any thread.
            bestMoveAtDepth = bestLocalMove;
            bestScoreAtDepth = bestLocalScore;
            // If the time limit expired during evaluation, stop
            // searching further depths.
            if (timeLimitMs > 0 && nextIndex.load() < (int)moves.size()) {
                // Return the best move found so far.
                chosenMove = bestMoveAtDepth;
                return chosenMove;
            }
        } else {
            // Sequential evaluation with an aspiration window centred
            // on the previous best score.  The window helps guide
            // alpha‑beta pruning at deeper depths.
            for (const Move& m : moves) {
                if (timeLimitMs > 0) {
                    auto now = clock::now();
                    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
                    if (elapsed >= timeLimitMs) {
                        // Return the best move found so far at this depth.
                        chosenMove = (currentDepth == 1 ? m : chosenMove);
                        return chosenMove;
                    }
                }
                Board child = makeMove(board, m);
                std::unordered_map<uint64_t,int> reps;
                reps[rootHash] = 1;
                int alpha = prevBestScore - aspirationWindow;
                int beta = prevBestScore + aspirationWindow;
                int score = minimax(child, currentDepth - 1, 1, alpha, beta,
                                    !board.whiteToMove, reps);
                if (score <= alpha || score >= beta) {
                    score = minimax(child, currentDepth - 1, 1, INT_MIN, INT_MAX,
                                    !board.whiteToMove, reps);
                }
                if (board.whiteToMove) {
                    if (score > bestScoreAtDepth) {
                        bestScoreAtDepth = score;
                        bestMoveAtDepth = m;
                    }
                } else {
                    if (score < bestScoreAtDepth) {
                        bestScoreAtDepth = score;
                        bestMoveAtDepth = m;
                    }
                }
            }
        }
        // After completing the current depth, update the chosen move.
        chosenMove = bestMoveAtDepth;
        // Update the previous best score for the next iteration.  This
        // centres the aspiration window on the most recent principal
        // variation value.
        prevBestScore = bestScoreAtDepth;
        // Check for time expiry after finishing the depth.  If the
        // allowed time is exceeded, break out of the iterative
        // deepening loop and return the best move found so far.
        if (timeLimitMs > 0) {
            auto now = clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
            if (elapsed >= timeLimitMs) {
                break;
            }
        }
    }
    return chosenMove;
}

} // namespace nikola
