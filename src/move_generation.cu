// Move generation routines for NikolaChess.
//
// These functions enumerate pseudo‑legal moves without considering check or
// special moves such as castling or en passant.  The goal is to
// produce candidate moves for a minimax search.  Because this file is
// compiled as CUDA code, the functions remain pure host functions but
// could be ported to device kernels if a parallel move generator were
// ever desirable.

#include "board.h"

namespace nikola {

// Helper to determine whether a coordinate lies on the board.
static inline bool onBoard(int r, int c) {
    return r >= 0 && r < 8 && c >= 0 && c < 8;
}

// Generate all moves for a pawn of the given colour at (r, c).
static void genPawnMoves(const Board& board, int r, int c, std::vector<Move>& moves) {
    int8_t piece = board.squares[r][c];
    bool white = piece > 0;
    int dir = white ? 1 : -1; // white moves to increasing row
    int startRow = white ? 1 : 6;
    int promotionRow = white ? 7 : 0;
    int nextRow = r + dir;
    // Single push
    if (onBoard(nextRow, c) && board.squares[nextRow][c] == EMPTY) {
        if (nextRow == promotionRow) {
            // Promotion to queen by default.  Other promotion pieces could be added here.
            Move m{r, c, nextRow, c, EMPTY, white ? WQ : BQ};
            moves.push_back(m);
        } else {
            Move m{r, c, nextRow, c, EMPTY, 0};
            moves.push_back(m);
        }
        // Double push from starting rank
        int twoRow = r + 2 * dir;
        if (r == startRow && board.squares[twoRow][c] == EMPTY) {
            Move m2{r, c, twoRow, c, EMPTY, 0};
            moves.push_back(m2);
        }
    }
    // Captures and promotions
    for (int dc : {-1, 1}) {
        int cr = r + dir;
        int cc = c + dc;
        if (onBoard(cr, cc)) {
            int8_t target = board.squares[cr][cc];
            // Normal capture
            if (target != EMPTY && (target > 0) != white) {
                if (cr == promotionRow) {
                    moves.push_back(Move{r, c, cr, cc, target, white ? WQ : BQ});
                } else {
                    moves.push_back(Move{r, c, cr, cc, target, 0});
                }
            }
        }
    }
    // En passant capture
    // If there is a target en passant column and pawn is on the fifth rank (white) or fourth rank (black)
    int epRow = white ? 4 : 3;
    if (board.enPassantCol >= 0 && r == epRow && std::abs(c - board.enPassantCol) == 1) {
        int cr = r + dir;
        int cc = board.enPassantCol;
        // The captured pawn is behind the destination square
        int8_t capturedPawn = white ? BP : WP;
        moves.push_back(Move{r, c, cr, cc, capturedPawn, 0});
    }
}

// Generate moves for a knight at (r, c).
static void genKnightMoves(const Board& board, int r, int c, std::vector<Move>& moves) {
    static const int offsets[8][2] = {{1,2},{2,1},{2,-1},{1,-2},{-1,-2},{-2,-1},{-2,1},{-1,2}};
    int8_t piece = board.squares[r][c];
    for (auto& off : offsets) {
        int rr = r + off[0];
        int cc = c + off[1];
        if (!onBoard(rr, cc)) continue;
        int8_t target = board.squares[rr][cc];
        if (target == EMPTY || (target > 0) != (piece > 0)) {
            Move m{r, c, rr, cc, target};
            moves.push_back(m);
        }
    }
}

// Generate sliding moves in given directions until blocked.
static void genSlidingMoves(const Board& board, int r, int c, const int directions[][2], int numDirs, std::vector<Move>& moves) {
    int8_t piece = board.squares[r][c];
    for (int i = 0; i < numDirs; ++i) {
        int dr = directions[i][0];
        int dc = directions[i][1];
        int rr = r + dr;
        int cc = c + dc;
        while (onBoard(rr, cc)) {
            int8_t target = board.squares[rr][cc];
            if (target == EMPTY) {
                moves.push_back(Move{r, c, rr, cc, EMPTY});
            } else {
                if ((target > 0) != (piece > 0)) {
                    moves.push_back(Move{r, c, rr, cc, target});
                }
                break;
            }
            rr += dr;
            cc += dc;
        }
    }
}

std::vector<Move> generateMoves(const Board& board) {
    std::vector<Move> candidateMoves;
    candidateMoves.reserve(64);
    bool white = board.whiteToMove;
    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 8; ++c) {
            int8_t piece = board.squares[r][c];
            if (piece == EMPTY) continue;
            if (white != (piece > 0)) continue;
            switch (piece) {
                case WP:
                case BP:
                    genPawnMoves(board, r, c, candidateMoves);
                    break;
                case WN:
                case BN:
                    genKnightMoves(board, r, c, candidateMoves);
                    break;
                case WB:
                case BB: {
                    static const int dirs[4][2] = {{1,1},{1,-1},{-1,1},{-1,-1}};
                    genSlidingMoves(board, r, c, dirs, 4, candidateMoves);
                    break;
                }
                case WR:
                case BR: {
                    static const int dirs[4][2] = {{1,0},{-1,0},{0,1},{0,-1}};
                    genSlidingMoves(board, r, c, dirs, 4, candidateMoves);
                    break;
                }
                case WQ:
                case BQ: {
                    static const int dirs[8][2] = {{1,0},{-1,0},{0,1},{0,-1},{1,1},{1,-1},{-1,1},{-1,-1}};
                    genSlidingMoves(board, r, c, dirs, 8, candidateMoves);
                    break;
                }
                case WK:
                case BK: {
                    static const int dirs[8][2] = {{1,0},{-1,0},{0,1},{0,-1},{1,1},{1,-1},{-1,1},{-1,-1}};
                    for (auto& d : dirs) {
                        int rr = r + d[0];
                        int cc = c + d[1];
                        if (!onBoard(rr, cc)) continue;
                        int8_t target = board.squares[rr][cc];
                        if (target == EMPTY || ((target > 0) != (piece > 0))) {
                            candidateMoves.push_back(Move{r, c, rr, cc, target, 0});
                        }
                    }
                    // Castling moves
                    if (piece == WK) {
                        // White kingside
                        if (board.whiteCanCastleKingSide &&
                            board.squares[0][5] == EMPTY && board.squares[0][6] == EMPTY &&
                            !isSquareAttacked(board, 0, 4, false) &&
                            !isSquareAttacked(board, 0, 5, false) &&
                            !isSquareAttacked(board, 0, 6, false)) {
                            candidateMoves.push_back(Move{0, 4, 0, 6, EMPTY, 0});
                        }
                        // White queenside
                        if (board.whiteCanCastleQueenSide &&
                            board.squares[0][1] == EMPTY && board.squares[0][2] == EMPTY && board.squares[0][3] == EMPTY &&
                            !isSquareAttacked(board, 0, 4, false) &&
                            !isSquareAttacked(board, 0, 3, false) &&
                            !isSquareAttacked(board, 0, 2, false)) {
                            candidateMoves.push_back(Move{0, 4, 0, 2, EMPTY, 0});
                        }
                    } else if (piece == BK) {
                        // Black kingside (row 7)
                        if (board.blackCanCastleKingSide &&
                            board.squares[7][5] == EMPTY && board.squares[7][6] == EMPTY &&
                            !isSquareAttacked(board, 7, 4, true) &&
                            !isSquareAttacked(board, 7, 5, true) &&
                            !isSquareAttacked(board, 7, 6, true)) {
                            candidateMoves.push_back(Move{7, 4, 7, 6, EMPTY, 0});
                        }
                        // Black queenside
                        if (board.blackCanCastleQueenSide &&
                            board.squares[7][1] == EMPTY && board.squares[7][2] == EMPTY && board.squares[7][3] == EMPTY &&
                            !isSquareAttacked(board, 7, 4, true) &&
                            !isSquareAttacked(board, 7, 3, true) &&
                            !isSquareAttacked(board, 7, 2, true)) {
                            candidateMoves.push_back(Move{7, 4, 7, 2, EMPTY, 0});
                        }
                    }
                    break;
                }
                default:
                    break;
            }
        }
    }
    // Filter out moves that leave own king in check
    std::vector<Move> legalMoves;
    legalMoves.reserve(candidateMoves.size());
    for (const Move& m : candidateMoves) {
        Board nb = makeMove(board, m);
        if (!isKingInCheck(nb, white)) {
            legalMoves.push_back(m);
        }
    }
    return legalMoves;
}

} // namespace nikola
