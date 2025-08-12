// Search algorithms for NikolaChess.
//
// Copyright (c) 2013 CPUTER Inc.
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

namespace nikola {

// Forward declaration of move generator and evaluation functions.
std::vector<Move> generateMoves(const Board& board);
int evaluateBoardCPU(const Board& board);


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
    // All other cases: assume sufficient material to continue.
    return false;
}

// Extended minimax with alpha‑beta pruning and draw detection.  The
// repetitions map counts occurrences of positions along the current
// search path.  When a position occurs three times, the game is
// drawn and zero is returned.  The half‑move clock is checked for
// the fifty‑move rule.  Insufficient material also yields a draw.
static int minimax(const nikola::Board& board, int depth, int alpha, int beta, bool maximizing,
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
    // Threefold repetition detection.  Compute the hash and update
    // count; if this is the third occurrence return draw.  We update
    // counts before descending so that deeper calls include the
    // current position.
    uint64_t h = computeZobrist(board);
    int& cnt = repetitions[h];
    cnt++;
    if (cnt >= 3) {
        cnt--;
        return 0;
    }
    // Depth‑limited search or no moves triggers static evaluation.
    if (depth == 0) {
        int val = evaluateBoardCPU(board);
        cnt--;
        return val;
    }
    auto moves = generateMoves(board);
    if (moves.empty()) {
        int val = evaluateBoardCPU(board);
        cnt--;
        return val;
    }
    int bestVal;
    if (maximizing) {
        bestVal = INT_MIN;
        for (const Move& m : moves) {
            Board child = makeMove(board, m);
            int eval = minimax(child, depth - 1, alpha, beta, false, repetitions);
            if (eval > bestVal) bestVal = eval;
            if (eval > alpha) alpha = eval;
            if (beta <= alpha) break;
        }
    } else {
        bestVal = INT_MAX;
        for (const Move& m : moves) {
            Board child = makeMove(board, m);
            int eval = minimax(child, depth - 1, alpha, beta, true, repetitions);
            if (eval < bestVal) bestVal = eval;
            if (eval < beta) beta = eval;
            if (beta <= alpha) break;
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
Move findBestMove(const Board& board, int depth) {
    Move bestMove{};
    int bestScore = board.whiteToMove ? INT_MIN : INT_MAX;
    auto moves = generateMoves(board);
    // If no moves are available, return a default move.
    if (moves.empty()) return bestMove;
    for (const Move& m : moves) {
        Board child = makeMove(board, m);
        // Initialise repetition table for this root move.  Start with
        // the current position counted once.  The child board will
        // update its own count when entering minimax.
        std::unordered_map<uint64_t,int> reps;
        reps[computeZobrist(board)] = 1;
        int score = minimax(child, depth - 1, INT_MIN, INT_MAX,
                            !board.whiteToMove, reps);
        if (board.whiteToMove) {
            if (score > bestScore) {
                bestScore = score;
                bestMove = m;
            }
        } else {
            if (score < bestScore) {
                bestScore = score;
                bestMove = m;
            }
        }
    }
    return bestMove;
}

} // namespace nikola
