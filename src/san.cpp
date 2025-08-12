// Standard Algebraic Notation (SAN) conversion for Supercomputer Chess Engine.
//
// Copyright (c) 2025 CPUTER Inc.
// All rights reserved.  See the LICENSE file for license terms.
//
// This implementation converts a move and the originating board
// into a Standard Algebraic Notation (SAN) string.  It handles
// castling, piece letters, captures, pawn captures, under‑
// promotions, disambiguation when multiple identical pieces can
// reach the destination and check/checkmate suffixes.  SAN
// strings follow the conventions described in the PGN standard:
// piece letters (N,B,R,Q,K), optional file/rank disambiguation for
// ambiguous moves, 'x' for captures, destination square, '=Q'
// (etc.) for promotions, and '+' or '#' to denote check or
// checkmate.  Stalemate is not explicitly marked as there is no
// notation for it in SAN.

#include "san.h"

namespace nikola {

std::string toSAN(const Board& board, const Move& m) {
    // Identify the moving piece and absolute type.
    int8_t piece = board.squares[m.fromRow][m.fromCol];
    int absPiece = piece >= 0 ? piece : -piece;
    // Detect castling: king moves two squares horizontally.
    if (absPiece == WK || absPiece == BK) {
        int colDiff = m.toCol - m.fromCol;
        if (colDiff == 2) {
            // King side castle
            // Check for check/mate on castling: do after constructing
            // SAN.  We append check indicators below.
            std::string san = "O-O";
            // Apply move to test for check/mate.
            Board child = makeMove(board, m);
            // Check if the move gives check to the opponent.
            bool givesCheck = isKingInCheck(child, child.whiteToMove);
            bool mate = false;
            if (givesCheck) {
                // Determine if opponent has any legal replies.
                auto replies = generateMoves(child);
                bool hasLegal = false;
                for (const Move& mv : replies) {
                    Board b2 = makeMove(child, mv);
                    // A reply is legal if it does not leave the moving side in check.
                    if (!isKingInCheck(b2, !b2.whiteToMove)) {
                        hasLegal = true;
                        break;
                    }
                }
                mate = !hasLegal;
            }
            if (givesCheck) {
                san += mate ? "#" : "+";
            }
            return san;
        } else if (colDiff == -2) {
            std::string san = "O-O-O";
            Board child = makeMove(board, m);
            bool givesCheck = isKingInCheck(child, child.whiteToMove);
            bool mate = false;
            if (givesCheck) {
                auto replies = generateMoves(child);
                bool hasLegal = false;
                for (const Move& mv : replies) {
                    Board b2 = makeMove(child, mv);
                    if (!isKingInCheck(b2, !b2.whiteToMove)) {
                        hasLegal = true;
                        break;
                    }
                }
                mate = !hasLegal;
            }
            if (givesCheck) {
                san += mate ? "#" : "+";
            }
            return san;
        }
    }
    std::string san;
    // Piece letter for non‑pawn moves.
    if (absPiece != WP && absPiece != BP) {
        char letter;
        switch (absPiece) {
            case WN: case BN: letter = 'N'; break;
            case WB: case BB: letter = 'B'; break;
            case WR: case BR: letter = 'R'; break;
            case WQ: case BQ: letter = 'Q'; break;
            case WK: case BK: letter = 'K'; break;
            default: letter = '?'; break;
        }
        san += letter;
        // Determine if disambiguation is needed.  Generate all
        // pseudo‑legal moves for the current board and look for
        // moves by the same piece type to the same destination.
        bool needFile = false;
        bool needRank = false;
        auto candidates = generateMoves(board);
        for (const Move& x : candidates) {
            if (x.fromRow == m.fromRow && x.fromCol == m.fromCol) continue;
            int8_t p2 = board.squares[x.fromRow][x.fromCol];
            if (p2 == 0) continue;
            int abs2 = p2 >= 0 ? p2 : -p2;
            if (abs2 != absPiece) continue;
            if (x.toRow == m.toRow && x.toCol == m.toCol) {
                // Another piece of same type can also move to the destination.
                if (x.fromCol != m.fromCol) needFile = true;
                if (x.fromRow != m.fromRow) needRank = true;
                // If both differ, we set both flags and break.
                if (needFile && needRank) break;
            }
        }
        if (needFile) {
            char f = static_cast<char>('a' + m.fromCol);
            san += f;
        }
        if (needRank) {
            char r = static_cast<char>('1' + m.fromRow);
            san += r;
        }
        // Capture indicator for non‑pawns.
        if (m.captured != EMPTY) {
            san += 'x';
        }
    } else {
        // Pawn moves: if capturing, include the file of the pawn.
        if (m.captured != EMPTY) {
            char file = static_cast<char>('a' + m.fromCol);
            san += file;
            san += 'x';
        }
    }
    // Add destination square.
    char toFile = static_cast<char>('a' + m.toCol);
    char toRank = static_cast<char>('1' + m.toRow);
    san += toFile;
    san += toRank;
    // Promotion.
    if (m.promotedTo != 0) {
        int absPromo = m.promotedTo > 0 ? m.promotedTo : -m.promotedTo;
        char promo;
        switch (absPromo) {
            case WN: case BN: promo = 'N'; break;
            case WB: case BB: promo = 'B'; break;
            case WR: case BR: promo = 'R'; break;
            case WQ: case BQ: default: promo = 'Q'; break;
        }
        san += '=';
        san += promo;
    }
    // Determine if the move gives check or checkmate.  Apply the
    // move to obtain the child position.  If the side to move is
    // in check after the move, append '+' or '#' accordingly.
    Board child = makeMove(board, m);
    bool givesCheck = isKingInCheck(child, child.whiteToMove);
    if (givesCheck) {
        // Determine if the opponent has any legal replies.  Iterate
        // over pseudo‑legal moves and test legality by ensuring the
        // move does not leave the moving side's king in check.
        auto replies = generateMoves(child);
        bool hasLegal = false;
        for (const Move& mv : replies) {
            Board b2 = makeMove(child, mv);
            // The side that just moved is !b2.whiteToMove; if that
            // side's king is in check, the move is illegal.
            if (!isKingInCheck(b2, !b2.whiteToMove)) {
                hasLegal = true;
                break;
            }
        }
        san += (hasLegal ? '+' : '#');
    }
    return san;
}

} // namespace nikola