#include "pv.h"
#include "tt_sharded.h"
#include "board.h"
#include <sstream>
#include <iomanip>
#include <cstdint>

namespace nikola {

// Simple 64-bit key for TT/PV purposes: mix piece squares and side to move.
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

std::vector<Move> extract_pv(const Board& root, int maxLen) {
    std::vector<Move> pv;
    Board b = root;
    for (int i=0;i<maxLen;++i) {
        auto k = key64(b);
        TTEntry e;
        if (!tt_lookup(k, e)) break;
        if (e.bestMove.fromRow == e.bestMove.toRow && e.bestMove.fromCol == e.bestMove.toCol) break;
        pv.push_back(e.bestMove);
        b = makeMove(b, e.bestMove);
    }
    return pv;
}

static char fileChar(int c){ return char('a'+c); }
static char rankChar(int r){ return char('1'+r); }

std::string move_to_uci(const Board& b, const Move& m) {
    (void)b; // if you later need promotion symbol, extend Move
    std::string s;
    s.push_back(fileChar(m.fromCol));
    s.push_back(rankChar(m.fromRow));
    s.push_back(fileChar(m.toCol));
    s.push_back(rankChar(m.toRow));
    // if (m.promotedTo) s.push_back(toLower(pieceChar(m.promotedTo)));
    return s;
}

} // namespace nikola
