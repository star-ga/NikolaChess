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
    int dir = piece > 0 ? 1 : -1; // white moves up (increasing row index)
    int startRow = piece > 0 ? 1 : 6;
    int nextRow = r + dir;
    // Single push
    if (onBoard(nextRow, c) && board.squares[nextRow][c] == EMPTY) {
        Move m{r, c, nextRow, c, EMPTY};
        moves.push_back(m);
        // Double push from starting rank
        int twoRow = r + 2 * dir;
        if (r == startRow && board.squares[twoRow][c] == EMPTY) {
            Move m2{r, c, twoRow, c, EMPTY};
            moves.push_back(m2);
        }
    }
    // Captures
    for (int dc : {-1, 1}) {
        int cr = r + dir;
        int cc = c + dc;
        if (onBoard(cr, cc)) {
            int8_t target = board.squares[cr][cc];
            if (target != EMPTY && (target > 0) != (piece > 0)) {
                Move m{r, c, cr, cc, target};
                moves.push_back(m);
            }
        }
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
    std::vector<Move> moves;
    moves.reserve(64);
    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 8; ++c) {
            int8_t piece = board.squares[r][c];
            if (piece == EMPTY) continue;
            // Only generate moves for side to move.
            if (board.whiteToMove != (piece > 0)) continue;
            switch (piece) {
                case WP:
                case BP:
                    genPawnMoves(board, r, c, moves);
                    break;
                case WN:
                case BN:
                    genKnightMoves(board, r, c, moves);
                    break;
                case WB:
                case BB: {
                    static const int dirs[4][2] = {{1,1},{1,-1},{-1,1},{-1,-1}};
                    genSlidingMoves(board, r, c, dirs, 4, moves);
                    break;
                }
                case WR:
                case BR: {
                    static const int dirs[4][2] = {{1,0},{-1,0},{0,1},{0,-1}};
                    genSlidingMoves(board, r, c, dirs, 4, moves);
                    break;
                }
                case WQ:
                case BQ: {
                    static const int dirs[8][2] = {{1,0},{-1,0},{0,1},{0,-1},{1,1},{1,-1},{-1,1},{-1,-1}};
                    genSlidingMoves(board, r, c, dirs, 8, moves);
                    break;
                }
                case WK:
                case BK: {
                    static const int dirs[8][2] = {{1,0},{-1,0},{0,1},{0,-1},{1,1},{1,-1},{-1,1},{-1,-1}};
                    for (auto& d : dirs) {
                        int rr = r + d[0];
                        int cc = c + d[1];
                        if (onBoard(rr, cc)) {
                            int8_t target = board.squares[rr][cc];
                            if (target == EMPTY || (target > 0) != (piece > 0)) {
                                moves.push_back(Move{r, c, rr, cc, target});
                            }
                        }
                    }
                    break;
                }
                default:
                    break;
            }
        }
    }
    return moves;
}

} // namespace nikola
