#include "multipv_search.h"
#include "engine_options.h"
#include "time_manager.h"
#include "pv.h"
#include "tt_sharded.h"
#include "tablebase.h"
#include <algorithm>
#include <chrono>
#include <iostream>

namespace nikola {

static constexpr int MATE = 30000;
static constexpr int INF  = 32000;

static uint64_t key64(const Board& b) {
    uint64_t k = 0x9E3779B97F4A7C15ULL;
    for (int r=0;r<8;++r){
        for (int c=0;c<8;++c){
            int v = b.squares[r][c];
            k ^= (uint64_t)((v & 0xFF) + 0x9E) + 0x9E3779B97F4A7C15ULL + (k<<6) + (k>>2);
        }
    }
    if (b.whiteToMove) k ^= 0xF00DFACEB00B5ULL;
    return k;
}

static inline bool isCapture(const Move& m){ return m.captured != EMPTY; }

// Lightweight alpha-beta that writes TT bestMove at each node for PV chaining.
static int alphabeta(Board& b, int depth, int alpha, int beta, int ply) {
    if (depth <= 0) {
        return evaluateBoardCPU(b);
    }

    int wdl = probeWDL(b); // existing semantics: 1 win, 0 draw, -1 loss, 2 unknown
    if (wdl != 2) {
        if (wdl == 1) return MATE - ply;
        if (wdl == -1) return -MATE + ply;
        return 0;
    }

    auto moves = generateMoves(b);
    if (moves.empty()) return 0; // stalemate/checkmate handled by eval in demo

    // Basic ordering: captures first
    std::stable_sort(moves.begin(), moves.end(), [&](const Move& a, const Move& c){
        return isCapture(a) && !isCapture(c);
    });

    int bestScore = -INF;
    Move bestMove = moves[0];
    for (const auto& m : moves) {
        Board nb = makeMove(b, m);
        int sc = -alphabeta(nb, depth-1, -beta, -alpha, ply+1);
        if (sc > bestScore) { bestScore = sc; bestMove = m; }
        if (sc > alpha) alpha = sc;
        if (alpha >= beta) break;
    }

    // store into TT for PV extraction
    TTEntry e; e.depth = depth; e.score = bestScore; e.flag = 0; e.bestMove = bestMove;
    tt_store(key64(b), e);
    return bestScore;
}

std::vector<RootResult> search_multipv(Board root, int N, int depthCap, int timeBudgetMs) {
    EngineOptions& o = opts();
    N = std::max(1, std::min(N, 8));

    // Strength cap
    if (o.LimitStrength) {
        int cap = 1 + o.Strength;
        if (depthCap <= 0 || depthCap > cap) depthCap = cap;
    }

    auto legal = generateMoves(root);
    if (legal.empty()) return {};

    // Order root: captures first
    std::stable_sort(legal.begin(), legal.end(), [&](const Move& a, const Move& c){
        return isCapture(a) && !isCapture(c);
    });

    auto t0 = std::chrono::steady_clock::now();
    auto deadline = (timeBudgetMs>0) ? (t0 + std::chrono::milliseconds(timeBudgetMs)) : std::chrono::steady_clock::time_point::max();

    std::vector<RootResult> out;
    out.reserve(N);

    for (int slot=0; slot<N && slot < (int)legal.size(); ++slot) {
        const Move cand = legal[slot];
        int bestScore = -INF;
        int lastScore = 0;
        Move chosen = cand;
        int targetDepth = (depthCap>0)? depthCap : 64;

        int alpha = -INF, beta = INF; // aspiration init
        int window = 50;

        for (int d=1; d<=targetDepth; ++d) {
            // aspiration around lastScore after first few depths
            if (d > 2) { alpha = lastScore - window; beta = lastScore + window; }
            Board b = makeMove(root, cand);
            int sc = alphabeta(b, d-1, alpha, beta, 1);
            // widen on fail
            if (sc <= alpha) { window *= 2; --d; continue; }
            if (sc >= beta)  { window *= 2; --d; continue; }
            lastScore = sc; bestScore = sc; chosen = cand;

            if (std::chrono::steady_clock::now() > deadline) break;
        }

        // panic extension: one more depth if time remains
        if (std::chrono::steady_clock::now() < deadline && targetDepth >= 6) {
            Board b = makeMove(root, cand);
            int sc = alphabeta(b, std::min(targetDepth,(depthCap>0?depthCap:targetDepth))+1 - 1, -INF, INF, 1);
            if (std::abs(sc) < INF) bestScore = sc;
        }

        // build PV via TT chaining
        std::vector<Move> pv = extract_pv(root, 60);
        if (pv.empty()) pv.push_back(chosen);

        out.push_back(RootResult{bestScore, chosen, pv});

        if (std::chrono::steady_clock::now() > deadline) break;
    }

    // sort best-first by score
    std::stable_sort(out.begin(), out.end(), [](const RootResult& A, const RootResult& B){ return A.scoreCentipawns > B.scoreCentipawns; });
    if ((int)out.size() > N) out.resize(N);
    return out;
}

} // namespace nikola
