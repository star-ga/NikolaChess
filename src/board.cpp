// Implementation of basic board manipulation functions for NikolaChess.
//
// Copyright (c) 2013 CPUTER Inc.
// All rights reserved.  See the LICENSE file for details.
#include "board.h"

#include <algorithm>

// Forward declaration of move generator.  Defined in move_generation.cu.
namespace nikola {
std::vector<Move> generateMoves(const Board& board);
}

namespace nikola {

Board initBoard() {
    Board b;
    // Fill the board with empty squares.
    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 8; ++c) {
            b.squares[r][c] = EMPTY;
        }
    }
    // Place white pieces.
    b.squares[0][0] = WR; b.squares[0][7] = WR;
    b.squares[0][1] = WN; b.squares[0][6] = WN;
    b.squares[0][2] = WB; b.squares[0][5] = WB;
    b.squares[0][3] = WQ; b.squares[0][4] = WK;
    for (int c = 0; c < 8; ++c) {
        b.squares[1][c] = WP;
    }
    // Place black pieces.
    b.squares[7][0] = BR; b.squares[7][7] = BR;
    b.squares[7][1] = BN; b.squares[7][6] = BN;
    b.squares[7][2] = BB; b.squares[7][5] = BB;
    b.squares[7][3] = BQ; b.squares[7][4] = BK;
    for (int c = 0; c < 8; ++c) {
        b.squares[6][c] = BP;
    }
    b.whiteToMove = true;

    // At the start of the game both sides may castle on both sides.
    b.whiteCanCastleKingSide = true;
    b.whiteCanCastleQueenSide = true;
    b.blackCanCastleKingSide = true;
    b.blackCanCastleQueenSide = true;
    b.enPassantCol = -1;
    // Initialise the half-move clock.  At the start of the game no
    // moves have been made, so the counter is zero.  This field
    // increments after non-capture, non-pawn moves and resets to
    // zero after a pawn move, capture or promotion.  It is used
    // to implement the fifty-move rule.
    b.halfMoveClock = 0;
    return b;
}

// Simple CPU evaluation used to validate GPU results.  It sums
// piece values and applies a small positional bonus for central
// pawn and knight placement.  Real engines employ much more
// sophisticated heuristics.
// Map a piece code to its material value.  These values are used by the
// evaluation function as part of the piece‑square tables.  Note that
// the piece codes are ±1 through ±6; the absolute value minus one
// yields an index into this array.
static const int materialValues[6] = {100, 320, 330, 500, 900, 100000};
static int pieceValue(int8_t p) {
    if (p == EMPTY) return 0;
    int idx = std::abs(p) - 1;
    int val = materialValues[idx];
    return (p > 0 ? val : -val);
}

// Piece‑square tables derived from the PeSTO evaluation function.  Each
// array has 64 entries corresponding to squares in rank‑file order.
// The middle‑game (mg) values are combined with the material values to
// yield mg_table[piece][square] for White and for Black (by flipping
// the square index).  See the Chessprogramming Wiki for details
//【406358174428833†L70-L95】.
static const int mg_pawn_table[64] = {
     0,   0,   0,   0,   0,   0,  0,   0,
    98, 134,  61,  95,  68, 126, 34, -11,
     -6,   7,  26,  31,  65,  56, 25, -20,
    -14,  13,   6,  21,  23,  12, 17, -23,
    -27,  -2,  -5,  12,  17,   6, 10, -25,
    -26,  -4,  -4, -10,   3,   3, 33, -12,
    -35,  -1, -20, -23, -15,  24, 38, -22,
     0,   0,   0,   0,   0,   0,  0,   0
};
static const int mg_knight_table[64] = {
    -167, -89, -34, -49,  61, -97, -15, -107,
     -73, -41,  72,  36,  23,  62,   7,  -17,
     -47,  60,  37,  65,  84, 129,  73,   44,
      -9,  17,  19,  53,  37,  69,  18,   22,
     -13,   4,  16,  13,  28,  19,  21,   -8,
     -23,  -9,  12,  10,  19,  17,  25,  -16,
     -29, -53, -12,  -3,  -1,  18, -14,  -19,
    -105, -21, -58, -33, -17, -28, -19,  -23
};
static const int mg_bishop_table[64] = {
    -29,   4, -82, -37, -25, -42,   7,  -8,
    -26,  16, -18, -13,  30,  59,  18, -47,
    -16,  37,  43,  40,  35,  50,  37,  -2,
     -4,   5,  19,  50,  37,  37,   7,  -2,
     -6,  13,  13,  26,  34,  12,  10,   4,
      0,  15,  15,  15,  14,  27,  18,  10,
      4,  15,  16,   0,   7,  21,  33,   1,
    -33,  -3, -14, -21, -13, -12, -39, -21
};
static const int mg_rook_table[64] = {
     32,  42,  32,  51, 63,  9,  31,  43,
     27,  32,  58,  62, 80, 67,  26,  44,
     -5,  19,  26,  36, 17, 45,  61,  16,
    -24, -11,   7,  26, 24, 35,  -8, -20,
    -36, -26, -12,  -1,  9, -7,   6, -23,
    -45, -25, -16, -17,  3,  0,  -5, -33,
    -44, -16, -20,  -9, -1, 11,  -6, -71,
    -19, -13,   1,  17, 16,  7, -37, -26
};
static const int mg_queen_table[64] = {
    -28,   0,  29,  12,  59,  44,  43,  45,
    -24, -39,  -5,   1, -16,  57,  28,  54,
    -13, -17,   7,   8,  29,  56,  47,  57,
    -27, -27, -16, -16,  -1,  17,  -2,   1,
     -9, -26,  -9, -10,  -2,  -4,   3,  -3,
    -14,   2, -11,  -2,  -5,   2,  14,   5,
    -35,  -8,  11,   2,   8,  15,  -3,   1,
     -1, -18,  -9,  10, -15, -25, -31, -50
};
static const int mg_king_table[64] = {
    -65,  23,  16, -15, -56, -34,   2,  13,
     29,  -1, -20,  -7,  -8,  -4, -38, -29,
     -9,  24,   2, -16, -20,   6,  22, -22,
    -17, -20, -12, -27, -30, -25, -14, -36,
    -49,  -1, -27, -39, -46, -44, -33, -51,
    -14, -14, -22, -46, -44, -30, -15, -27,
      1,   7,  -8, -64, -43, -16,   9,   8,
    -15,  36,  12, -54,   8, -28,  24,  14
};

// Flip a square index vertically.  Used to generate Black's piece‑square
// table from White's by mirroring the board along the horizontal axis.
static inline int flipSq(int sq) {
    int rank = sq / 8;
    int file = sq % 8;
    return (7 - rank) * 8 + file;
}

// Compute the middle‑game piece‑square table for both colours and
// combine material values.  The resulting table has dimensions
// [2][6][64] where index 0 is White, 1 is Black, and 6 piece types
// are ordered pawn, knight, bishop, rook, queen, king.  This is
// initialised on first use.
static void initPestoTables(int mgTable[2][6][64]) {
    static bool initialised = false;
    static int cached[2][6][64];
    if (initialised) {
        // Copy cached values into the provided table.
        for (int c = 0; c < 2; ++c) {
            for (int p = 0; p < 6; ++p) {
                for (int sq = 0; sq < 64; ++sq) {
                    mgTable[c][p][sq] = cached[c][p][sq];
                }
            }
        }
        return;
    }
    // Local arrays of pointers to the mg piece tables for convenience.
    const int* pieceTables[6] = {mg_pawn_table, mg_knight_table, mg_bishop_table,
                                 mg_rook_table, mg_queen_table, mg_king_table};
    for (int p = 0; p < 6; ++p) {
        for (int sq = 0; sq < 64; ++sq) {
            // White table: material + square bonus as given.
            cached[0][p][sq] = materialValues[p] + pieceTables[p][sq];
            // Black table: material + square bonus on flipped square.
            int fsq = flipSq(sq);
            cached[1][p][sq] = materialValues[p] + pieceTables[p][fsq];
        }
    }
    initialised = true;
    // Copy into mgTable output.
    for (int c = 0; c < 2; ++c) {
        for (int p = 0; p < 6; ++p) {
            for (int sq = 0; sq < 64; ++sq) {
                mgTable[c][p][sq] = cached[c][p][sq];
            }
        }
    }
}

// Neural network evaluation stub.  This demonstrates how a neural
// network can be integrated into the evaluation pipeline.  A real
// engine would train weights offline and embed them here; we use
// simple deterministic weights for demonstration.  The network takes
// 768 binary input features (12 piece types × 64 squares) flattened
// into a vector.  We implement one hidden layer with 32 neurons and
// ReLU activations, and a single output neuron.  The output is
// interpreted as a score in centipawns.
static int nnEvaluateBoard(const Board& board) {
    // Define dimensions.
    const int inputSize = 12 * 64;
    const int hiddenSize = 32;
    // Static weight matrices.  These are pseudo‑random constants to
    // illustrate the mechanism.  In practice they should be learned
    // from data.  We initialise them on first use.
    static bool weightsInit = false;
    static float w1[32][inputSize];
    static float b1[32];
    static float w2[32];
    static float b2;
    if (!weightsInit) {
        // Initialise weights with small deterministic values based on
        // simple pseudo‑random sequence.  Using a linear congruential
        // generator ensures reproducibility across runs.
        unsigned int seed = 42;
        auto randf = [&seed]() {
            seed = seed * 1664525u + 1013904223u;
            return (seed & 0xFFFF) / 65535.0f - 0.5f;
        };
        for (int i = 0; i < hiddenSize; ++i) {
            b1[i] = randf() * 0.1f;
            for (int j = 0; j < inputSize; ++j) {
                w1[i][j] = randf() * 0.01f;
            }
        }
        for (int i = 0; i < hiddenSize; ++i) {
            w2[i] = randf() * 0.01f;
        }
        b2 = randf() * 0.1f;
        weightsInit = true;
    }
    // Construct input vector.  We encode each square with a one‑hot
    // representation over 12 piece types.  Index mapping: piece codes
    // ±1..±6 map to 0..11; White pieces occupy indices 0..5 and
    // Black pieces occupy 6..11.  Empty squares contribute zero.
    float hidden[32] = {0};
    // Feedforward first layer: accumulate w1[i][j] * x[j] + b1[i].
    for (int i = 0; i < hiddenSize; ++i) {
        float sum = b1[i];
        // Loop over board squares and accumulate weights for
        // corresponding feature indices.  This avoids constructing
        // the entire 768‑element input vector explicitly.
        for (int r = 0; r < 8; ++r) {
            for (int c = 0; c < 8; ++c) {
                int8_t p = board.squares[r][c];
                if (p == EMPTY) continue;
                int type = (p > 0 ? p - 1 : (-p - 1));
                int colorIdx = (p > 0 ? 0 : 1);
                int featureIdx = colorIdx * 6 * 64 + type * 64 + (r * 8 + c);
                sum += w1[i][featureIdx];
            }
        }
        // ReLU activation.
        hidden[i] = sum > 0 ? sum : 0;
    }
    // Output layer.
    float out = b2;
    for (int i = 0; i < hiddenSize; ++i) {
        out += w2[i] * hidden[i];
    }
    // Convert to integer score.  Scale by 100 centipawns for small
    // values.  Clamp to avoid integer overflow.
    int score = static_cast<int>(out * 100);
    if (score > 100000) score = 100000;
    if (score < -100000) score = -100000;
    return score;
}

int evaluateBoardCPU(const Board& board) {
    // Switch between traditional heuristic evaluation and neural network
    // evaluation.  A real engine might toggle this via a command‑line
    // option or configuration flag.  Here we expose a compile‑time
    // constant for simplicity.
    constexpr bool useNeuralNet = false;
    if (useNeuralNet) {
        return nnEvaluateBoard(board);
    }
    // Initialize piece‑square table on first use.
    int mgTable[2][6][64];
    initPestoTables(mgTable);
    int scoreWhite = 0;
    int scoreBlack = 0;
    // Sum piece‑square values for each side.
    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 8; ++c) {
            int8_t p = board.squares[r][c];
            if (p == EMPTY) continue;
            int type = (p > 0 ? p - 1 : (-p - 1));
            int colorIdx = (p > 0 ? 0 : 1);
            int sq = r * 8 + c;
            int val = mgTable[colorIdx][type][sq];
            if (p > 0) {
                scoreWhite += val;
            } else {
                scoreBlack += val;
            }
        }
    }
    int eval = scoreWhite - scoreBlack;
    // Mobility: number of legal moves available to each side.  We
    // generate moves for the current position from both sides’
    // perspective by toggling the side to move.  Greater mobility is
    // rewarded.  The weight controls the influence on the final score.
    const int mobilityWeight = 5;
    // Count moves for White.
    Board tmp = board;
    tmp.whiteToMove = true;
    int whiteMoves = static_cast<int>(generateMoves(tmp).size());
    // Count moves for Black.
    tmp.whiteToMove = false;
    int blackMoves = static_cast<int>(generateMoves(tmp).size());
    eval += mobilityWeight * (whiteMoves - blackMoves);
    return eval;
}

} // namespace nikola
