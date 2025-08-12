// Copyright (c) 2025 CPUTER Inc.
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
// Enumerated piece codes.  Unlike the original demonstration which
// overloaded piece values with evaluation scores, the piece codes here
// are simple identifiers in the range ±1 through ±6.  Positive values
// represent White pieces and negative values represent Black pieces.
// A separate evaluation table maps these identifiers to material values
// and positional bonuses.  Keeping the codes distinct makes it
// straightforward to index into piece‑square tables and to generate
// underpromotions.
enum Piece : int8_t {
    EMPTY = 0,
    WP = 1,  // White pawn
    WN = 2,  // White knight
    WB = 3,  // White bishop
    WR = 4,  // White rook
    WQ = 5,  // White queen
    WK = 6,  // White king
    BP = -1, // Black pawn
    BN = -2, // Black knight
    BB = -3, // Black bishop
    BR = -4, // Black rook
    BQ = -5, // Black queen
    BK = -6  // Black king
};

// Structure representing a chess move.  The engine uses zero‑based row
// and column indices.  The captured field stores the value of the piece
// captured on the destination square (zero if no capture).  The
// promotedTo field indicates the piece code that a pawn is promoted
// into (zero if no promotion).  This structure contains enough
// information to undo moves during search.
struct Move {
    int fromRow;
    int fromCol;
    int toRow;
    int toCol;
    int captured;
    int promotedTo;
};

// Board representation.  In addition to the squares array we track
// which side is to move.  Castling and en passant are not fully
// implemented in this demonstration, but room is left for future
// expansion.  The simple structure can be copied directly to the GPU.
struct Board {
    int8_t squares[8][8];
    bool whiteToMove;
    // Castling rights.  true if the corresponding side can still castle on
    // that side.  Castling rights are updated when the king or
    // appropriate rook moves.
    bool whiteCanCastleKingSide;
    bool whiteCanCastleQueenSide;
    bool blackCanCastleKingSide;
    bool blackCanCastleQueenSide;
    // enPassantCol stores the file (0-7) on which an en passant
    // capture is possible following a two‑square pawn advance; -1 if
    // none.
    int8_t enPassantCol;

    // Half move clock counts the number of half moves (ply) since the
    // last capture or pawn move.  It is used to implement the
    // fifty‑move rule: if this value reaches 100 both sides can claim
    // a draw.  The clock is reset to zero on any capture, pawn move or
    // promotion.
    int halfMoveClock;
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

// Compute the perft (performance test) count for the given board.
// Returns the number of leaf nodes reachable from the current position
// at the specified depth.  This function is defined in perft.cpp.
uint64_t perft(const Board& board, int depth);

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

// Return a new Board that results from applying the given move to
// `board`.  This routine updates castling rights and en passant
// information, handles pawn promotion and en passant capture, and
// toggles the side to move.  It does not perform legality checks;
// those should be done separately.
Board makeMove(const Board& board, const Move& m);

// Determine whether the square at (row, col) is attacked by the
// specified side.  `byWhite` should be true if testing attacks by
// White, false for attacks by Black.  This routine considers
// pseudo‑legal moves (ignoring pins).  It is used to implement
// legality checks for castling and to detect check conditions.
bool isSquareAttacked(const Board& board, int row, int col, bool byWhite);

// Return true if the king belonging to the specified side is in
// check.  This routine locates the king on the board and calls
// `isSquareAttacked` on its square.  White king is tested when
// `white` is true, black king when false.
bool isKingInCheck(const Board& board, bool white);


} // namespace nikola
