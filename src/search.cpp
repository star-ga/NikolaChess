// Search algorithms for NikolaChess.
//
// Copyright (c) 2025 CPUTER Inc.
// All rights reserved.  See the LICENSE file for license terms.
//
// This module contains a simple minimax search with alpha-beta
// pruning.  It operates on copies of the Board structure and
// generates moves using the functions in move_generation.cu.  At leaf
// nodes the static evaluation is obtained by calling the CPU
// evaluation; this could be replaced by GPU evaluation for deeper
// parallel searches.  Because the goal of this demonstration is to
// illustrate how GPU acceleration can be integrated, the search depth
// is kept modest.

#include "board.h"
#include "tt_entry.h"
#include "tt_sharded.h"
#include "thread_affinity.h"

#include <climits>
#include <chrono>
#include <cstdlib>
#include <mutex>
#include <thread>
#include <atomic>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <random>
#include <algorithm>
#include <future>   // for std::future in staticEvaluate()
#include <iostream>

#include "pv.h"

// Include tablebase probing functions and piece counting utility.  The
// engine will consult endgame tablebases when the position has
// sufficiently few pieces.  If a tablebase result is available, it
// overrides the heuristic evaluation.
#include "tablebase.h"

// Include the micro-batcher for GPU evaluation.  The batcher
// collects evaluation requests from multiple threads and processes
// them in batches on the GPU.  See micro_batcher.h for details.
#include "micro_batcher.h"
#include "see.h"

namespace nikola {

// Forward declaration of move generator and evaluation functions.
std::vector<Move> generateMoves(const Board& board);
int evaluateBoardCPU(const Board& board);
std::vector<int> evaluateBoardsGPU(const Board* boards, int nBoards);

// Enumeration of available evaluation backends.  By default the
// engine uses the CPU implementation (NNUE or classical).  If
// NIKOLA_GPU environment variable is set to a non-zero value, the
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

// UCI option: number of principal variations to display (1..8)
int g_multiPV = 1;

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
    // submit the board to the batcher and wait for the result.
    if (g_evalBackend == EvaluationBackend::GPU_BATCHED && g_batcher) {
        try {
            std::future<int> fut = g_batcher->submit(b);
            return fut.get();
        } catch (...) {
            // Fall back to CPU evaluation on any failure.
        }
    }
    // Default path: evaluate on the CPU.
    return evaluateBoardCPU(b);
}


// The following code extends the basic minimax implementation with
// detection of draw conditions: threefold repetition, the fifty-move
// rule and insufficient material.  We implement Zobrist hashing to
// uniquely identify positions (including side to move, castling
// rights and en passant targets) and track the number of times each
// position has occurred along the current search path.  When a
// position appears three times, the function returns a score of zero
// (draw).  The half-move clock stored in the Board tracks the
// number of half moves since the last capture or pawn move; if this
// reaches 100, the fifty-move rule applies and the game is drawn.

namespace {

// Compute a Zobrist hash for the given board.  The hash includes the
// side to move, castling rights, en passant target file and the
// placement of all pieces.  We initialise the random tables on first
// use with a deterministic seed to ensure reproducibility.
static uint64_t computeZobrist(const nikola::Board& board) {
    using nikola::Piece;
    // Static tables of random values.  Index 0..5 for White pieces
    // (pawn..king) and 6..11 for Black pieces.  The 64 squares are
    // numbered 0..63 in rank-file order.
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
    // Side to move
    if (board.whiteToMove) h ^= sideToMoveKey;
    return h;
}

// Determine whether a position has insufficient material to force a
// checkmate.  Returns true if both sides lack sufficient force to
// deliver mate according to simplified heuristics.
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

    int whiteMinor = whiteBishops + whiteKnights;
    int blackMinor = blackBishops + blackKnights;

    // King vs king.
    if (whiteMinor == 0 && blackMinor == 0) return true;
    // K+B vs K or K+N vs K.
    if ((whiteMinor == 1 && blackMinor == 0) || (whiteMinor == 0 && blackMinor == 1)) {
        return true;
    }
    // K+B vs K+B with same-color bishops.
    if (whiteBishops == 1 && blackBishops == 1 && whiteKnights == 0 && blackKnights == 0) {
        if (whiteBishopColors == blackBishopColors) return true;
    }
    // K+N vs K+N.
    if (whiteKnights == 1 && blackKnights == 1 && whiteBishops == 0 && blackBishops == 0) {
        return true;
    }
    // Two knights vs bare king is insufficient.
    if (whiteKnights == 2 && whiteBishops == 0 && whitePawns + whiteRooks + whiteQueens == 0 &&
        blackKnights == 0 && blackBishops == 0 && blackPawns + blackRooks + blackQueens == 0) {
        return true;
    }
    if (blackKnights == 2 && blackBishops == 0 && blackPawns + blackRooks + blackQueens == 0 &&
        whiteKnights == 0 && whiteBishops == 0 && whitePawns + whiteRooks + whiteQueens == 0) {
        return true;
    }
    return false;
}

// TT flags (kept for compatibility with stored values).
static const int TT_EXACT = 0;
static const int TT_LOWERBOUND = 1;
static const int TT_UPPERBOUND = 2;

// Quiescence search: captures/promotions only.
static int quiescence(const Board& board, int alpha, int beta, bool maximizing) {
    int standPat = staticEvaluate(board);
    if (maximizing) {
        if (standPat >= beta) return standPat;
        if (standPat > alpha) alpha = standPat;
    } else {
        if (standPat <= alpha) return standPat;
        if (standPat < beta) beta = standPat;
    }
    auto moves = generateMoves(board);
    for (const Move& m : moves) {
        if (m.captured == nikola::EMPTY && m.promotedTo == 0) continue;
        Board child = makeMove(board, m);
        int score = quiescence(child, alpha, beta, !maximizing);
        if (maximizing) {
            if (score > alpha) alpha = score;
            if (alpha >= beta) return alpha;
        } else {
            if (score < beta) beta = score;
            if (beta <= alpha) return beta;
        }
    }
    return maximizing ? alpha : beta;
}

// Material values for ordering (pawn..king).
static const int orderingMaterial[6] = {100, 320, 330, 500, 900, 100000};

// Killer/history/countermove heuristics (thread-local).
thread_local static Move killerMoves[64][2];
thread_local static int historyTable[64][64];
thread_local static Move counterMoves[64][64];

static inline int orderingPieceValue(int8_t p) {
    int idx = std::abs(p) - 1;
    return orderingMaterial[idx];
}

// Minimax with alpha-beta + draws + heuristics.
static int minimax(const nikola::Board& board, int depth, int ply, int alpha, int beta, bool maximizing,
                   std::unordered_map<uint64_t,int>& repetitions, const nikola::Move* prevMove) {
    // 50-move rule
    if (board.halfMoveClock >= 100) return 0;
    // Insufficient material
    if (isInsufficientMaterial(board)) return 0;

    uint64_t h = computeZobrist(board);

    // Threefold repetition
    int& cnt = repetitions[h];
    cnt++;
    if (cnt >= 3) { cnt--; return 0; }

    // Transposition table lookup
    TTEntry ttEntry;
    bool ttFound = tt_lookup(h, ttEntry);
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
    }

    // Null-move pruning (outside TT scope, as intended)
    if (depth >= 2) {
        bool inCheck = isKingInCheck(board, board.whiteToMove);
        if (!inCheck) {
            int numPieces = nikola::countPieces(board);
            if (numPieces > 3) {
                int R = (depth > 6 ? 3 : 2);
                if (depth - 1 - R >= 0) {
                    Board nullBoard = board;
                    nullBoard.whiteToMove = !board.whiteToMove;
                    nullBoard.enPassantCol = -1;
                    nullBoard.halfMoveClock = board.halfMoveClock + 1;

                    std::unordered_map<uint64_t,int> repsNull = repetitions;
                    int nullScore = minimax(nullBoard, depth - 1 - R, ply + 1, alpha, beta,
                                            !maximizing, repsNull, prevMove);

                    bool prune = false;
                    if (maximizing) {
                        if (nullScore >= beta) {
                            int verifyDepth = depth - 1 - R - 1;
                            if (verifyDepth < 0) verifyDepth = 0;
                            std::unordered_map<uint64_t,int> repsVerify = repetitions;
                            int verifyScore = minimax(nullBoard, verifyDepth, ply + 1, beta - 1, beta,
                                                      !maximizing, repsVerify, prevMove);
                            if (verifyScore >= beta) prune = true;
                        }
                    } else {
                        if (nullScore <= alpha) {
                            int verifyDepth = depth - 1 - R - 1;
                            if (verifyDepth < 0) verifyDepth = 0;
                            std::unordered_map<uint64_t,int> repsVerify = repetitions;
                            int verifyScore = minimax(nullBoard, verifyDepth, ply + 1, alpha, alpha + 1,
                                                      !maximizing, repsVerify, prevMove);
                            if (verifyScore <= alpha) prune = true;
                        }
                    }
                    if (prune) { cnt--; return nullScore; }
                }
            }
        }
      // ProbCut using static evaluation to prune unlikely branches
      if (depth >= 3) {
          int staticScore = staticEvaluate(board);
          int margin = 200 + 50 * depth;
          if (maximizing) {
              if (staticScore - margin >= beta) { cnt--; return staticScore; }
          } else {
              if (staticScore + margin <= alpha) { cnt--; return staticScore; }
          }
      }
    }

    // Quiescence at depth 0
    if (depth == 0) {
        int val = quiescence(board, alpha, beta, maximizing);
        cnt--;
        return val;
    }

    auto moves = generateMoves(board);
    if (moves.empty()) {
        int val = quiescence(board, alpha, beta, maximizing);
        cnt--;
        return val;
    }

    // Futility & simple razoring
    if (depth <= 2) {
        bool inCheck = isKingInCheck(board, board.whiteToMove);
        if (!inCheck) {
            int staticScore = staticEvaluate(board);
            int margin = 50 * depth;
            if (maximizing) {
                if (staticScore + margin <= alpha) { cnt--; return staticScore; }
            } else {
                if (staticScore - margin >= beta) { cnt--; return staticScore; }
            }
            int razorMargin = 150;
            if (depth == 2) {
                if (maximizing && staticScore + razorMargin <= alpha) { cnt--; return staticScore; }
                if (!maximizing && staticScore - razorMargin >= beta) { cnt--; return staticScore; }
            }
        }
    }

    // PV move from TT (codex variant with validity check)
    Move pvMove{};
    bool hasPV = false;
    {
        TTEntry pvEntry;
        if (tt_lookup(h, pvEntry)) {
            pvMove = pvEntry.bestMove;
            if (!(pvMove.fromRow == 0 && pvMove.fromCol == 0 &&
                  pvMove.toRow == 0 && pvMove.toCol == 0)) {
                hasPV = true;
            }
        }
    }

    // Score moves for ordering
    std::vector<std::pair<int, Move>> scored;
    scored.reserve(moves.size());
    for (const Move& m : moves) {
        int score = 0;
        // PV bonus
        if (hasPV && m.fromRow == pvMove.fromRow && m.fromCol == pvMove.fromCol &&
            m.toRow == pvMove.toRow && m.toCol == pvMove.toCol && m.promotedTo == pvMove.promotedTo) {
            score += 1000000;
        }
        // Promotion bonus
        if (m.promotedTo != 0) {
            int idx = std::abs(m.promotedTo) - 1;
            score += 10000 + orderingMaterial[idx];
        }
        // Capture bonus via SEE + MVV/LVA-ish tweak
        if (m.captured != nikola::EMPTY) {
            int seeScore = see(board, m);
            score += 1000 * seeScore;
            int capturedVal = orderingPieceValue((int8_t)m.captured);
            int attackerVal = orderingPieceValue(board.squares[m.fromRow][m.fromCol]);
            score += 10 * capturedVal - attackerVal;
        }
        // Killer moves
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
        // History heuristic
        int fromIdx = m.fromRow * 8 + m.fromCol;
        int toIdx = m.toRow * 8 + m.toCol;
        score += historyTable[fromIdx][toIdx];

        // Countermove bonus
        if (prevMove != nullptr) {
            int pFrom = prevMove->fromRow * 8 + prevMove->fromCol;
            int pTo = prevMove->toRow * 8 + prevMove->toCol;
            const Move& cm = counterMoves[pFrom][pTo];
            if (m.fromRow == cm.fromRow && m.fromCol == cm.fromCol &&
                m.toRow == cm.toRow && m.toCol == cm.toCol && m.promotedTo == cm.promotedTo) {
                score += 850000;
            }
        }
        scored.emplace_back(score, m);
    }

    std::stable_sort(scored.begin(), scored.end(), [](const auto& a, const auto& b) {
        return a.first > b.first;
    });

    int bestVal;
    Move bestChildMove{};
    int alphaOrig = alpha;
    int betaOrig = beta;

    if (maximizing) {
        bestVal = INT_MIN;
        for (size_t idx = 0; idx < scored.size(); ++idx) {
            const Move& m = scored[idx].second;
            Board child = makeMove(board, m);
            int nextDepth = depth - 1;
            int extension = 0;
            if (idx == 0 && depth >= 3) {
                int shallow = minimax(child, depth - 2, ply + 1, alpha, beta, false, repetitions, &m);
                if (shallow <= alpha) extension = 1;
            }
            nextDepth += extension;
            int eval;
            bool isQuiet = (m.captured == nikola::EMPTY && m.promotedTo == 0);
            if (nextDepth > 0 && isQuiet && idx >= 3 && depth >= 3) {
                int reduction = 1;
                if (depth > 6 && idx >= 6) reduction = 2;
                int reducedDepth = nextDepth - reduction;
                if (reducedDepth < 0) reducedDepth = 0;
                int evalReduced = minimax(child, reducedDepth, ply + 1, alpha, beta, false, repetitions, &m);
                eval = evalReduced;
                if (evalReduced > alpha) {
                    eval = minimax(child, nextDepth, ply + 1, alpha, beta, false, repetitions, &m);
                }
            } else {
                eval = minimax(child, nextDepth, ply + 1, alpha, beta, false, repetitions, &m);
            }
            if (eval > bestVal) {
                bestVal = eval;
                bestChildMove = m;
            }
            if (eval > alpha) alpha = eval;
            if (beta <= alpha) {
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
                if (prevMove != nullptr) {
                    int pFrom = prevMove->fromRow * 8 + prevMove->fromCol;
                    int pTo = prevMove->toRow * 8 + prevMove->toCol;
                    counterMoves[pFrom][pTo] = m;
                }
                break; // beta cutoff
            }
        }
    } else {
        bestVal = INT_MAX;
        for (size_t idx = 0; idx < scored.size(); ++idx) {
            const Move& m = scored[idx].second;
            Board child = makeMove(board, m);
            int nextDepth = depth - 1;
            int extension = 0;
            if (idx == 0 && depth >= 3) {
                int shallow = minimax(child, depth - 2, ply + 1, alpha, beta, true, repetitions, &m);
                if (shallow >= beta) extension = 1;
            }
            nextDepth += extension;
            int eval;
            bool isQuiet = (m.captured == nikola::EMPTY && m.promotedTo == 0);
            if (nextDepth > 0 && isQuiet && idx >= 3 && depth >= 3) {
                int reduction = 1;
                if (depth > 6 && idx >= 6) reduction = 2;
                int reducedDepth = nextDepth - reduction;
                if (reducedDepth < 0) reducedDepth = 0;
                int evalReduced = minimax(child, reducedDepth, ply + 1, alpha, beta, true, repetitions, &m);
                eval = evalReduced;
                if (evalReduced < beta) {
                    eval = minimax(child, nextDepth, ply + 1, alpha, beta, true, repetitions, &m);
                }
            } else {
                eval = minimax(child, nextDepth, ply + 1, alpha, beta, true, repetitions, &m);
            }
            if (eval < bestVal) {
                bestVal = eval;
                bestChildMove = m;
            }
            if (eval < beta) beta = eval;
            if (beta <= alpha) {
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
                if (prevMove != nullptr) {
                    int pFrom = prevMove->fromRow * 8 + prevMove->fromCol;
                    int pTo = prevMove->toRow * 8 + prevMove->toCol;
                    counterMoves[pFrom][pTo] = m;
                }
                break; // alpha cutoff
            }
        }
    }

    // Store TT entry
    int flag;
    if (bestVal <= alphaOrig) {
        flag = TT_UPPERBOUND;
    } else if (bestVal >= betaOrig) {
        flag = TT_LOWERBOUND;
    } else {
        flag = TT_EXACT;
    }
    TTEntry newEntry{depth, bestVal, flag, bestChildMove};
    tt_store(h, newEntry);

    cnt--;
    return bestVal;
}

} // end anonymous namespace

// Choose the best move for the current player using minimax search to a
// given depth.
Move findBestMove(const Board& board, int depth, int timeLimitMs) {
    tt_configure_from_env();
    Move chosenMove{};
    { tt_clear(); }

    struct RootScore { Move move; int score; };
    std::vector<RootScore> finalRootScores;

    // Reset heuristics
    for (int i = 0; i < 64; ++i) {
        for (int k = 0; k < 2; ++k) killerMoves[i][k] = Move{};
        for (int j = 0; j < 64; ++j) {
            historyTable[i][j] = 0;
            counterMoves[i][j] = Move{};
        }
    }

    auto initialMoves = generateMoves(board);
    if (initialMoves.empty()) return chosenMove;

    // Time management
    if (timeLimitMs <= 0) {
        int defaultMs = 3000;
        const char* envRem = std::getenv("NIKOLA_REMAINING_MS");
        const char* envInc = std::getenv("NIKOLA_INCREMENT_MS");
        int remaining = envRem ? std::atoi(envRem) : 0;
        int increment = envInc ? std::atoi(envInc) : 0;
        if (remaining > 0) {
            int alloc = remaining / 30 + increment / 2;
            if (alloc < 50) alloc = 50;
            timeLimitMs = alloc;
        } else {
            timeLimitMs = defaultMs;
        }
    }

    using clock = std::chrono::steady_clock;
    auto start = clock::now();

    int numThreads = 1;
    if (const char* envThr = std::getenv("NIKOLA_THREADS")) {
        int t = std::atoi(envThr);
        if (t > 1) numThreads = t;
    }

    int prevBestScore = 0;
    int aspirationWindow = 50;

    for (int currentDepth = 1; currentDepth <= depth; ++currentDepth) {
        Move bestMoveAtDepth{};
        int bestScoreAtDepth = board.whiteToMove ? INT_MIN : INT_MAX;
        std::vector<RootScore> rootScores;

        auto moves = generateMoves(board);

        // Try to put PV first (codex variant with validity check)
        uint64_t rootHash = computeZobrist(board);
        Move pv{};
        bool pvExists = false;
        {
            TTEntry pvEntry;
            if (tt_lookup(rootHash, pvEntry)) {
                pv = pvEntry.bestMove;
                if (!(pv.fromRow == 0 && pv.fromCol == 0 && pv.toRow == 0 && pv.toCol == 0)) {
                    pvExists = true;
                }
            }
        }
        if (pvExists) {
            for (size_t i = 0; i < moves.size(); ++i) {
                const Move& m = moves[i];
                if (m.fromRow == pv.fromRow && m.fromCol == pv.fromCol &&
                    m.toRow == pv.toRow && m.toCol == pv.toCol && m.promotedTo == pv.promotedTo) {
                    if (i != 0) std::swap(moves[0], moves[i]);
                    break;
                }
            }
        }

        if (numThreads > 1 && moves.size() > 1) {
            std::atomic<int> nextIndex(0);
            std::atomic<bool> timeUp(false);
            std::mutex bestMutex;
            std::mutex scoreMutex;
            Move bestLocalMove{};
            int bestLocalScore = board.whiteToMove ? INT_MIN : INT_MAX;

            auto worker = [&](int tid) {
                const char* pinEnv = std::getenv("NIKOLA_PIN_THREADS");
                if (pinEnv && pinEnv[0] == '1') { nikola_pin_thread_to_core(tid); }
                for (int i = 0; i < 64; ++i) {
                    for (int k = 0; k < 2; ++k) killerMoves[i][k] = Move{};
                    for (int j = 0; j < 64; ++j) {
                        historyTable[i][j] = 0;
                        counterMoves[i][j] = Move{};
                    }
                }
                while (true) {
                    if (timeLimitMs > 0) {
                        auto now = clock::now();
                        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
                        if (elapsed >= timeLimitMs) { timeUp.store(true); break; }
                    }
                    int idx = nextIndex.fetch_add(1);
                    if (idx >= (int)moves.size() || timeUp.load()) break;

                    const Move& m = moves[idx];
                    Board child = makeMove(board, m);
                    std::unordered_map<uint64_t,int> reps;
                    reps[rootHash] = 1;

                    int score = minimax(child, currentDepth - 1, 1, INT_MIN, INT_MAX,
                                        !board.whiteToMove, reps, &m);
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
                    if (currentDepth == depth) {
                        std::lock_guard<std::mutex> g(scoreMutex);
                        rootScores.push_back({m, score});
                    }
                }
            };

            std::vector<std::thread> threads;
            threads.reserve(numThreads - 1);
            for (int t = 1; t < numThreads; ++t) threads.emplace_back(worker, t);
            worker(0);
            for (auto& th : threads) { if (th.joinable()) th.join(); }

            bestMoveAtDepth = bestLocalMove;
            bestScoreAtDepth = bestLocalScore;

            if (timeLimitMs > 0 && nextIndex.load() < (int)moves.size()) {
                chosenMove = bestMoveAtDepth;
                std::cout << "bestmove " << move_to_uci(board, chosenMove) << std::endl;
                return chosenMove;
            }
        } else {
            for (const Move& m : moves) {
                if (timeLimitMs > 0) {
                    auto now = clock::now();
                    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
                    if (elapsed >= timeLimitMs) {
                        chosenMove = (currentDepth == 1 ? m : chosenMove);
                        std::cout << "bestmove " << move_to_uci(board, chosenMove) << std::endl;
                        return chosenMove;
                    }
                }
                Board child = makeMove(board, m);
                std::unordered_map<uint64_t,int> reps;
                reps[rootHash] = 1;

                int alpha = prevBestScore - aspirationWindow;
                int beta  = prevBestScore + aspirationWindow;
                int score = minimax(child, currentDepth - 1, 1, alpha, beta,
                                    !board.whiteToMove, reps, &m);
                if (score <= alpha || score >= beta) {
                    score = minimax(child, currentDepth - 1, 1, INT_MIN, INT_MAX,
                                    !board.whiteToMove, reps, &m);
                }
                if (board.whiteToMove) {
                    if (score > bestScoreAtDepth) { bestScoreAtDepth = score; bestMoveAtDepth = m; }
                } else {
                    if (score < bestScoreAtDepth) { bestScoreAtDepth = score; bestMoveAtDepth = m; }
                }
                if (currentDepth == depth) {
                    rootScores.push_back({m, score});
                }
            }
        }

        if (currentDepth == depth) {
            auto comp = board.whiteToMove ?
                [](const RootScore& a, const RootScore& b){ return a.score > b.score; } :
                [](const RootScore& a, const RootScore& b){ return a.score < b.score; };
            std::sort(rootScores.begin(), rootScores.end(), comp);
            if ((int)rootScores.size() > g_multiPV) rootScores.resize(g_multiPV);
            finalRootScores = rootScores;
            if (!rootScores.empty()) {
                bestMoveAtDepth = rootScores[0].move;
                bestScoreAtDepth = rootScores[0].score;
            }
        }

        chosenMove = bestMoveAtDepth;
        prevBestScore = bestScoreAtDepth;

        if (timeLimitMs > 0) {
            auto now = clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
            if (elapsed >= timeLimitMs) break;
        }
    }

    if (!finalRootScores.empty()) {
        for (size_t i = 0; i < finalRootScores.size(); ++i) {
            const auto& rs = finalRootScores[i];
            std::cout << "info multipv " << (i+1) << ' ';
            if (std::abs(rs.score) > 29000) {
                int mate_in = (30000 - std::abs(rs.score) + 1) / 2;
                if (rs.score < 0) mate_in = -mate_in;
                std::cout << "score mate " << mate_in << ' ';
            } else {
                std::cout << "score cp " << rs.score << ' ';
            }
            std::cout << "pv " << move_to_uci(board, rs.move) << std::endl;
        }
        std::cout << "bestmove " << move_to_uci(board, finalRootScores[0].move) << std::endl;
        return finalRootScores[0].move;
    }

    std::cout << "bestmove " << move_to_uci(board, chosenMove) << std::endl;
    return chosenMove;
}

} // namespace nikola
