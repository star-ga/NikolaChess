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
#include <chrono>

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
    // Two knights vs bare king is insufficient to force mate.  If one side
    // has exactly two knights and the other has no minor pieces, it is a draw.
    if (whiteKnights == 2 && whiteBishops == 0 && whitePawns + whiteRooks + whiteQueens == 0 &&
        blackKnights == 0 && blackBishops == 0 && blackPawns + blackRooks + blackQueens == 0) {
        return true;
    }
    if (blackKnights == 2 && blackBishops == 0 && blackPawns + blackRooks + blackQueens == 0 &&
        whiteKnights == 0 && whiteBishops == 0 && whitePawns + whiteRooks + whiteQueens == 0) {
        return true;
    }
    // All other cases: assume sufficient material to continue.
    return false;
}

// Transposition table entry.  Stores the depth at which a position
// was evaluated, the evaluation score, a bound flag and the best
// move found from this position.  The flag takes one of three
// values: EXACT indicates the stored score is the precise evaluation;
// LOWERBOUND means the score is a lower bound (alpha cutoff);
// UPPERBOUND means the score is an upper bound (beta cutoff).
struct TTEntry {
    int depth;
    int score;
    int flag;
    nikola::Move bestMove;
};
static const int TT_EXACT = 0;
static const int TT_LOWERBOUND = 1;
static const int TT_UPPERBOUND = 2;
static std::unordered_map<uint64_t, TTEntry> transTable;

// Piece material values used for move ordering.  These mirror the
// values used in the evaluation function: pawn = 100, knight = 320,
// bishop = 330, rook = 500, queen = 900, king = 100000.  We use
// these values to prioritise captures and promotions during search.
static const int orderingMaterial[6] = {100, 320, 330, 500, 900, 100000};

// Helper to obtain the material value of a piece code.  Input must
// be non‑zero.  The absolute piece code minus one indexes into the
// orderingMaterial array.  White and black pieces have the same
// absolute value.
static inline int orderingPieceValue(int8_t p) {
    int idx = std::abs(p) - 1;
    return orderingMaterial[idx];
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
    // Compute the Zobrist hash once for both repetition detection and
    // transposition table lookup.
    uint64_t h = computeZobrist(board);
    // Threefold repetition detection.  Update count for the current
    // position.  If this is the third occurrence, return a draw.
    int& cnt = repetitions[h];
    cnt++;
    if (cnt >= 3) {
        cnt--;
        return 0;
    }
    // Check the transposition table for a cached result at sufficient
    // depth.  If found, use the stored bounds to narrow the window or
    // return an exact evaluation.  Note: transposition table entries
    // do not account for repetition counts or 50‑move rule, so draw
    // detection happens before TT lookup.
    auto ttIt = transTable.find(h);
    if (ttIt != transTable.end() && ttIt->second.depth >= depth) {
        const TTEntry& entry = ttIt->second;
        if (entry.flag == TT_EXACT) {
            cnt--;
            return entry.score;
        } else if (entry.flag == TT_LOWERBOUND) {
            if (entry.score > alpha) alpha = entry.score;
        } else if (entry.flag == TT_UPPERBOUND) {
            if (entry.score < beta) beta = entry.score;
        }
        if (alpha >= beta) {
            cnt--;
            return entry.score;
        }
    }
    // If depth is zero or no moves, perform static evaluation.  We do
    // not consult the TT again here because shallow positions are
    // inexpensive to evaluate and we still need to adjust the
    // repetition count.
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
    // Determine principal variation (PV) move from the TT if available
    // to improve move ordering.  This move should be searched first.
    Move pvMove{};
    bool hasPV = false;
    if (ttIt != transTable.end()) {
        pvMove = ttIt->second.bestMove;
        // Only consider PV if it has a non‑zero from square (valid move).
        if (!(pvMove.fromRow == 0 && pvMove.fromCol == 0 && pvMove.toRow == 0 && pvMove.toCol == 0)) {
            hasPV = true;
        }
    }
    // Assign ordering scores to moves.  Moves that capture a high
    // valued piece or promote are ordered first.  The principal
    // variation move receives the highest priority.
    std::vector<std::pair<int, Move>> scored;
    scored.reserve(moves.size());
    for (const Move& m : moves) {
        int score = 0;
        // PV move bonus
        if (hasPV && m.fromRow == pvMove.fromRow && m.fromCol == pvMove.fromCol &&
            m.toRow == pvMove.toRow && m.toCol == pvMove.toCol && m.promotedTo == pvMove.promotedTo) {
            score += 1000000;
        }
        // Promotion bonus
        if (m.promotedTo != 0) {
            // Promotions are highly valuable.  Use the value of the
            // promoted piece (minus pawn) to scale.
            int idx = std::abs(m.promotedTo) - 1;
            score += 10000 + orderingMaterial[idx];
        }
        // Capture bonus using MVV‑LVA heuristic: prioritize captures
        // of high valued pieces with low valued attackers.
        if (m.captured != nikola::EMPTY) {
            int capturedVal = orderingPieceValue((int8_t)m.captured);
            int attackerVal = orderingPieceValue(board.squares[m.fromRow][m.fromCol]);
            score += 10 * capturedVal - attackerVal;
        }
        scored.emplace_back(score, m);
    }
    // Sort moves by descending score to improve pruning.  A stable
    // sort ensures the relative order of equally scored moves
    // remains deterministic.
    std::stable_sort(scored.begin(), scored.end(), [](const auto& a, const auto& b) {
        return a.first > b.first;
    });
    int bestVal;
    Move bestChildMove{};
    // Save original alpha and beta to determine the TT flag later.
    int alphaOrig = alpha;
    int betaOrig = beta;
    if (maximizing) {
        bestVal = INT_MIN;
        for (const auto& pair : scored) {
            const Move& m = pair.second;
            Board child = makeMove(board, m);
            int eval = minimax(child, depth - 1, alpha, beta, false, repetitions);
            if (eval > bestVal) {
                bestVal = eval;
                bestChildMove = m;
            }
            if (eval > alpha) alpha = eval;
            if (beta <= alpha) break; // beta cutoff
        }
    } else {
        bestVal = INT_MAX;
        for (const auto& pair : scored) {
            const Move& m = pair.second;
            Board child = makeMove(board, m);
            int eval = minimax(child, depth - 1, alpha, beta, true, repetitions);
            if (eval < bestVal) {
                bestVal = eval;
                bestChildMove = m;
            }
            if (eval < beta) beta = eval;
            if (beta <= alpha) break; // alpha cutoff
        }
    }
    // Store the result in the transposition table.  Only store
    // positions evaluated at this depth or deeper to avoid
    // overwriting more accurate information.  Determine the flag
    // based on the relation of bestVal to the original alpha/beta
    // window.
    int flag;
    if (bestVal <= alphaOrig) {
        flag = TT_UPPERBOUND;
    } else if (bestVal >= betaOrig) {
        flag = TT_LOWERBOUND;
    } else {
        flag = TT_EXACT;
    }
    TTEntry newEntry{depth, bestVal, flag, bestChildMove};
    auto existing = transTable.find(h);
    if (existing == transTable.end() || existing->second.depth < depth) {
        transTable[h] = newEntry;
    }
    // Restore repetition count when backtracking.
    cnt--;
    return bestVal;
}

// Choose the best move for the current player using minimax search to a
// given depth.  Returns the move that yields the highest (for White)
// or lowest (for Black) evaluation.  If there are no legal moves,
// returns a zeroed move.
Move findBestMove(const Board& board, int depth, int timeLimitMs) {
    Move chosenMove{};
    // Clear the transposition table to avoid interference from
    // previous searches.  We retain the table across iterative
    // deepening depths to allow re‑use of work from shallower
    // searches but clear it on each new top‑level search.
    transTable.clear();
    // If no moves are available, return an empty move immediately.
    auto initialMoves = generateMoves(board);
    if (initialMoves.empty()) return chosenMove;
    // Track the start time when a time limit is provided.  We use
    // steady_clock to avoid issues with system clock adjustments.
    using clock = std::chrono::steady_clock;
    auto start = clock::now();
    // Iterative deepening: search incrementally deeper and keep track
    // of the best move at each depth.  The principal variation
    // information stored in the transposition table will guide move
    // ordering in deeper searches.  If a time limit is set and
    // exceeded, we stop searching and return the best move found
    // thus far.
    for (int currentDepth = 1; currentDepth <= depth; ++currentDepth) {
        Move bestMoveAtDepth{};
        int bestScoreAtDepth = board.whiteToMove ? INT_MIN : INT_MAX;
        // Re‑generate root moves at each depth because the transposition
        // table may have learned a new principal variation.  Use
        // simple ordering: search the PV move first if available.
        auto moves = generateMoves(board);
        // Try to move the PV move (if any) to the front of the list.
        uint64_t rootHash = computeZobrist(board);
        auto itPV = transTable.find(rootHash);
        Move pv{};
        bool pvExists = false;
        if (itPV != transTable.end()) {
            pv = itPV->second.bestMove;
            if (!(pv.fromRow == 0 && pv.fromCol == 0 && pv.toRow == 0 && pv.toCol == 0)) {
                pvExists = true;
            }
        }
        if (pvExists) {
            // Find pv in moves and move it to the front.
            for (size_t i = 0; i < moves.size(); ++i) {
                const Move& m = moves[i];
                if (m.fromRow == pv.fromRow && m.fromCol == pv.fromCol &&
                    m.toRow == pv.toRow && m.toCol == pv.toCol && m.promotedTo == pv.promotedTo) {
                    if (i != 0) {
                        std::swap(moves[0], moves[i]);
                    }
                    break;
                }
            }
        }
        // Evaluate each move at the current depth.
        for (const Move& m : moves) {
            // Time check: if a time limit is set and exceeded, break
            // out of the search.  We perform the check at the
            // beginning of each iteration to ensure a responsive
            // cutoff.
            if (timeLimitMs > 0) {
                auto now = clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
                if (elapsed >= timeLimitMs) {
                    // Return the best move found so far at this depth.
                    chosenMove = (currentDepth == 1 ? m : chosenMove);
                    return chosenMove;
                }
            }
            Board child = makeMove(board, m);
            // Initialise repetition table for this root move.  Start with
            // the current position counted once.  The child board will
            // update its own count when entering minimax.
            std::unordered_map<uint64_t,int> reps;
            reps[rootHash] = 1;
            int score = minimax(child, currentDepth - 1, INT_MIN, INT_MAX,
                                !board.whiteToMove, reps);
            if (board.whiteToMove) {
                if (score > bestScoreAtDepth) {
                    bestScoreAtDepth = score;
                    bestMoveAtDepth = m;
                }
            } else {
                if (score < bestScoreAtDepth) {
                    bestScoreAtDepth = score;
                    bestMoveAtDepth = m;
                }
            }
        }
        // After completing the current depth, update the chosen move.
        chosenMove = bestMoveAtDepth;
        // Check for time expiry after finishing the depth.  If the
        // allowed time is exceeded, break out of the iterative
        // deepening loop and return the best move found so far.
        if (timeLimitMs > 0) {
            auto now = clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
            if (elapsed >= timeLimitMs) {
                break;
            }
        }
    }
    return chosenMove;
}

} // namespace nikola
