// Copyright (c) 2025
//
// This header defines the core data structures for the NikolaChess engine.
// The engine uses a simple 8x8 array representation for the chess board and
// enumerated integer values to represent each piece type.  Positive values
// denote white pieces and negative values denote black pieces.  Empty
// squares are zero.  Keeping the representation compact makes it easy
// to copy entire board states to the GPU for parallel evaluation.

#pragma once

#include <vector>
#include <cstdint>

namespace nikola {

// Enumeration of piece codes.  White pieces are positive, black pieces
// negative.  The numeric values roughly correspond to relative piece
// strength to make evaluation code slightly more intuitive.
enum Piece : int8_t {
    EMPTY = 0,
    WP = 1,  // White pawn
    WN = 3,  // White knight
    WB = 3,  // White bishop
    WR = 5,  // White rook
    WQ = 9,  // White queen
    WK = 100, // White king (large value for checkmate detection)
    BP = -1, // Black pawn
    BN = -3, // Black knight
    BB = -3, // Black bishop
    BR = -5, // Black rook
    BQ = -9, // Black queen
    BK = -100 // Black king
};

// Structure representing a chess move.  The engine uses zero‑based row
// and column indices.  The captured field stores the value of the piece
// captured on the destination square (zero if no capture).  This
// information is sufficient to undo moves during search.
struct Move {
    int fromRow;
    int fromCol;
    int toRow;
    int toCol;
    int captured;
};

// Board representation.  In addition to the squares array we track
// which side is to move.  Castling and en passant are not fully
// implemented in this demonstration, but room is left for future
// expansion.  The simple structure can be copied directly to the GPU.
struct Board {
    int8_t squares[8][8];
    bool whiteToMove;
};

// Initialise a Board to the standard chess starting position.  Pawns
// occupy the second and seventh ranks, rooks on the corners, knights
// beside them, bishops next, with queen on her colour.  White moves
// first.
Board initBoard();

// Generate all pseudo‑legal moves for the given board.  This function
// ignores check detection and special rules like castling or en
// passant.  Its purpose in the demonstration engine is to produce
// candidate moves for a minimax search.  The implementation lives in
// move_generation.cu.
std::vector<Move> generateMoves(const Board& board);

// Evaluate a batch of boards on the GPU.  The boards array should
// contain nBoards consecutive Board objects.  The function returns a
// vector of integer scores, one per board, where positive values
// favour White and negative values favour Black.  The actual kernel
// implementation resides in evaluate.cu.  Note that evaluation uses
// simple material and positional heuristics for demonstration purposes.
std::vector<int> evaluateBoardsGPU(const Board* boards, int nBoards);

// Compute a static evaluation of a single board on the CPU.  This
// helper is useful for verifying the correctness of the GPU evaluation
// routine and for fallback on systems without a compatible GPU.
int evaluateBoardCPU(const Board& board);

} // namespace nikola
