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


// Recursive minimax with alpha‑beta pruning.  Depth is counted in
// plies (half moves).  When depth reaches zero or there are no
// moves, the board is statically evaluated.  Maximising player is
// White, minimising player is Black.
static int minimax(const Board& board, int depth, int alpha, int beta, bool maximizing) {
    if (depth == 0) {
        return evaluateBoardCPU(board);
    }
    auto moves = generateMoves(board);
    if (moves.empty()) {
        // No legal moves: return static evaluation (checkmate or stalemate not detected)
        return evaluateBoardCPU(board);
    }
    if (maximizing) {
        int maxEval = INT_MIN;
        for (const Move& m : moves) {
            Board child = makeMove(board, m);
            int eval = minimax(child, depth - 1, alpha, beta, false);
            if (eval > maxEval) maxEval = eval;
            if (eval > alpha) alpha = eval;
            if (beta <= alpha) break; // beta cutoff
        }
        return maxEval;
    } else {
        int minEval = INT_MAX;
        for (const Move& m : moves) {
            Board child = makeMove(board, m);
            int eval = minimax(child, depth - 1, alpha, beta, true);
            if (eval < minEval) minEval = eval;
            if (eval < beta) beta = eval;
            if (beta <= alpha) break; // alpha cutoff
        }
        return minEval;
    }
}

// Choose the best move for the current player using minimax search to a
// given depth.  Returns the move that yields the highest (for White)
// or lowest (for Black) evaluation.  If there are no legal moves,
// returns a zeroed move.
Move findBestMove(const Board& board, int depth) {
    Move bestMove{};
    int bestScore = board.whiteToMove ? INT_MIN : INT_MAX;
    auto moves = generateMoves(board);
    for (const Move& m : moves) {
        Board child = makeMove(board, m);
        int score = minimax(child, depth - 1, INT_MIN, INT_MAX, !board.whiteToMove);
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
