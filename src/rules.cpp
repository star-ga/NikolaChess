// Chess rules utilities for NikolaChess.
//
// Copyright (c) 2025 CPUTER Inc.
// All rights reserved.  See the LICENSE file for license terms.
//
// This module implements rule enforcement needed for a functional
// chess engine: applying moves with updates to castling rights and
// en passant state, detecting attacks on a given square and
// determining whether a king is in check.  These functions are used
// both by the move generator to filter illegal moves and by the
// search logic.

#include "board.h"

#include <algorithm>
#include <cstdlib>

namespace nikola {

// Helper to determine whether a coordinate lies on the board.
static inline bool onBoard(int r, int c) {
    return r >= 0 && r < 8 && c >= 0 && c < 8;
}

Board makeMove(const Board& board, const Move& m) {
    Board nb = board;
    // Update the half-move clock.  The half-move clock counts the
    // number of half moves (plies) since the last pawn move,
    // capture or promotion.  It increments by one on a quiet move
    // and resets to zero on any pawn move, capture or promotion.  We
    // perform this update before manipulating the board so that the
    // old board's values are visible.
    nb.halfMoveClock = board.halfMoveClock;
    int8_t piece = nb.squares[m.fromRow][m.fromCol];
    // Reset enPassant.  It will be set again for pawn double moves.
    nb.enPassantCol = -1;
    // Castling: move rook as appropriate and clear castling rights.
    bool isKing = (piece == WK || piece == BK);
    if (isKing) {
        if (piece > 0) {
            nb.whiteCanCastleKingSide = false;
            nb.whiteCanCastleQueenSide = false;
        } else {
            nb.blackCanCastleKingSide = false;
            nb.blackCanCastleQueenSide = false;
        }
        // Kingside castling: king moves two squares to the right.
        if (m.toCol - m.fromCol == 2) {
            // Move rook from h-file (col 7) to f-file (col 5)
            nb.squares[m.fromRow][7] = EMPTY;
            nb.squares[m.fromRow][5] = (piece > 0 ? WR : BR);
        } else if (m.toCol - m.fromCol == -2) {
            // Queenside castling: rook from a-file to d-file.
            nb.squares[m.fromRow][0] = EMPTY;
            nb.squares[m.fromRow][3] = (piece > 0 ? WR : BR);
        }
    }
    // Update castling rights if a rook moves.
    if (piece == WR) {
        if (m.fromRow == 0 && m.fromCol == 0) nb.whiteCanCastleQueenSide = false;
        if (m.fromRow == 0 && m.fromCol == 7) nb.whiteCanCastleKingSide = false;
    } else if (piece == BR) {
        if (m.fromRow == 7 && m.fromCol == 0) nb.blackCanCastleQueenSide = false;
        if (m.fromRow == 7 && m.fromCol == 7) nb.blackCanCastleKingSide = false;
    }
    // Update castling rights if a rook is captured.
    if (m.captured != EMPTY) {
        int8_t cap = m.captured;
        if (cap == WR) {
            if (m.toRow == 0 && m.toCol == 0) nb.whiteCanCastleQueenSide = false;
            if (m.toRow == 0 && m.toCol == 7) nb.whiteCanCastleKingSide = false;
        } else if (cap == BR) {
            if (m.toRow == 7 && m.toCol == 0) nb.blackCanCastleQueenSide = false;
            if (m.toRow == 7 && m.toCol == 7) nb.blackCanCastleKingSide = false;
        }
    }
    // Remove piece from source square.
    nb.squares[m.fromRow][m.fromCol] = EMPTY;
    // Handle en passant capture.  If pawn moves diagonally into empty
    // square and the board at destination is empty, capture the pawn
    // behind.
    bool isPawn = (piece == WP || piece == BP);
    bool isDiagonal = (m.fromCol != m.toCol);
    if (isPawn && isDiagonal && board.squares[m.toRow][m.toCol] == EMPTY) {
        // Remove the pawn that moved two squares last turn.
        int pawnRow = (piece > 0) ? m.toRow - 1 : m.toRow + 1;
        nb.squares[pawnRow][m.toCol] = EMPTY;
    }
    // Place piece or promotion on destination.
    if (m.promotedTo != 0) {
        nb.squares[m.toRow][m.toCol] = (int8_t)m.promotedTo;
    } else {
        nb.squares[m.toRow][m.toCol] = piece;
    }
    // Set en passant target if pawn moves two squares.
    if (isPawn && std::abs(m.toRow - m.fromRow) == 2) {
        nb.enPassantCol = m.fromCol;
    }
    // Toggle side to move.
    nb.whiteToMove = !board.whiteToMove;

    // Update half-move clock after determining whether this move
    // constitutes a capture, pawn move or promotion.  We use the
    // original move fields rather than inspecting the modified board.
    bool pawnMove = (piece == WP || piece == BP);
    bool isCapture = (m.captured != EMPTY);
    bool isPromotion = (m.promotedTo != 0);
    if (pawnMove || isCapture || isPromotion) {
        nb.halfMoveClock = 0;
    } else {
        // Increment the counter for a quiet move.
        nb.halfMoveClock += 1;
    }
    updateBitboards(nb);
    return nb;
}

// Determine whether square (row,col) is attacked by white or black.
bool isSquareAttacked(const Board& board, int row, int col, bool byWhite) {
    int pawnDir = byWhite ? 1 : -1;
    // Check pawn attacks
    int pawnRow = row - pawnDir;
    if (onBoard(pawnRow, col - 1)) {
        int8_t p = board.squares[pawnRow][col - 1];
        if (byWhite ? (p == WP) : (p == BP)) return true;
    }
    if (onBoard(pawnRow, col + 1)) {
        int8_t p = board.squares[pawnRow][col + 1];
        if (byWhite ? (p == WP) : (p == BP)) return true;
    }
    // Check knight attacks
    static const int knightOffsets[8][2] = {{1,2},{2,1},{2,-1},{1,-2},{-1,-2},{-2,-1},{-2,1},{-1,2}};
    for (auto& off : knightOffsets) {
        int rr = row + off[0];
        int cc = col + off[1];
        if (!onBoard(rr, cc)) continue;
        int8_t p = board.squares[rr][cc];
        if (byWhite ? (p == WN) : (p == BN)) return true;
    }
    // Check sliding pieces: rooks/queens and bishops/queens
    // Directions for rook/queen
    static const int rookDirs[4][2] = {{1,0},{-1,0},{0,1},{0,-1}};
    for (auto& d : rookDirs) {
        int rr = row + d[0];
        int cc = col + d[1];
        while (onBoard(rr, cc)) {
            int8_t p = board.squares[rr][cc];
            if (p != EMPTY) {
                if (byWhite) {
                    if (p == WR || p == WQ) return true;
                } else {
                    if (p == BR || p == BQ) return true;
                }
                break;
            }
            rr += d[0];
            cc += d[1];
        }
    }
    // Directions for bishop/queen
    static const int bishopDirs[4][2] = {{1,1},{1,-1},{-1,1},{-1,-1}};
    for (auto& d : bishopDirs) {
        int rr = row + d[0];
        int cc = col + d[1];
        while (onBoard(rr, cc)) {
            int8_t p = board.squares[rr][cc];
            if (p != EMPTY) {
                if (byWhite) {
                    if (p == WB || p == WQ) return true;
                } else {
                    if (p == BB || p == BQ) return true;
                }
                break;
            }
            rr += d[0];
            cc += d[1];
        }
    }
    // Check king attacks (adjacent squares)
    static const int kingDirs[8][2] = {{1,0},{-1,0},{0,1},{0,-1},{1,1},{1,-1},{-1,1},{-1,-1}};
    for (auto& d : kingDirs) {
        int rr = row + d[0];
        int cc = col + d[1];
        if (!onBoard(rr, cc)) continue;
        int8_t p = board.squares[rr][cc];
        if (byWhite ? (p == WK) : (p == BK)) return true;
    }
    return false;
}

bool isKingInCheck(const Board& board, bool white) {
    // Locate the king
    int kingRow = -1, kingCol = -1;
    int8_t kingPiece = white ? WK : BK;
    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 8; ++c) {
            if (board.squares[r][c] == kingPiece) {
                kingRow = r;
                kingCol = c;
                break;
            }
        }
        if (kingRow != -1) break;
    }
    if (kingRow == -1) {
        // Should not happen in a valid position
        return false;
    }
    return isSquareAttacked(board, kingRow, kingCol, !white);
}

} // namespace nikola
