#include "board.h"
#include <cstdint>

// Forward declarations of Fathom probing functions.
extern "C" unsigned tb_probe_wdl(
    uint64_t white,
    uint64_t black,
    uint64_t kings,
    uint64_t queens,
    uint64_t rooks,
    uint64_t bishops,
    uint64_t knights,
    uint64_t pawns,
    unsigned rule50,
    unsigned castling,
    unsigned ep,
    bool turn);

extern "C" unsigned tb_probe_root(
    uint64_t white,
    uint64_t black,
    uint64_t kings,
    uint64_t queens,
    uint64_t rooks,
    uint64_t bishops,
    uint64_t knights,
    uint64_t pawns,
    unsigned rule50,
    unsigned castling,
    unsigned ep,
    bool turn,
    unsigned* results);

extern "C" int tb_probe_dtz(
    uint64_t white,
    uint64_t black,
    uint64_t kings,
    uint64_t queens,
    uint64_t rooks,
    uint64_t bishops,
    uint64_t knights,
    uint64_t pawns,
    unsigned rule50,
    unsigned castling,
    unsigned ep,
    bool turn);

namespace nikola {

namespace {
// Helper to build bitboards from Board representation.
struct TBBitboards {
    uint64_t white = 0;
    uint64_t black = 0;
    uint64_t kings = 0;
    uint64_t queens = 0;
    uint64_t rooks = 0;
    uint64_t bishops = 0;
    uint64_t knights = 0;
    uint64_t pawns = 0;
};

TBBitboards build_bitboards(const Board& b) {
    TBBitboards bb;
    const Bitboards& src = b.bitboards;

    bb.white = src.whiteMask;
    bb.black = src.blackMask;

    bb.kings   = src.pieces[5] | src.pieces[11];
    bb.queens  = src.pieces[4] | src.pieces[10];
    bb.rooks   = src.pieces[3] | src.pieces[9];
    bb.bishops = src.pieces[2] | src.pieces[8];
    bb.knights = src.pieces[1] | src.pieces[7];
    bb.pawns   = src.pieces[0] | src.pieces[6];

    return bb;
}

unsigned castling_mask(const Board& b) {
    unsigned mask = 0;
    if (b.whiteCanCastleKingSide) mask |= 1u;
    if (b.whiteCanCastleQueenSide) mask |= 2u;
    if (b.blackCanCastleKingSide) mask |= 4u;
    if (b.blackCanCastleQueenSide) mask |= 8u;
    return mask;
}

unsigned ep_square(const Board& b) {
    if (b.enPassantCol == -1)
        return 0u;
    unsigned row = b.whiteToMove ? 5u : 2u; // rank 6 or 3
    return row * 8u + static_cast<unsigned>(b.enPassantCol);
}
} // namespace

unsigned tbProbeWDL(const Board& b) {
    TBBitboards bb = build_bitboards(b);
    unsigned castle = castling_mask(b);
    unsigned ep = ep_square(b);
    return ::tb_probe_wdl(bb.white, bb.black, bb.kings, bb.queens, bb.rooks,
                          bb.bishops, bb.knights, bb.pawns, b.halfMoveClock,
                          castle, ep, b.whiteToMove);
}

unsigned tbProbeRoot(const Board& b, unsigned* results) {
    TBBitboards bb = build_bitboards(b);
    unsigned castle = castling_mask(b);
    unsigned ep = ep_square(b);
    return ::tb_probe_root(bb.white, bb.black, bb.kings, bb.queens, bb.rooks,
                           bb.bishops, bb.knights, bb.pawns, b.halfMoveClock,
                           castle, ep, b.whiteToMove, results);
}

int tbProbeDTZ(const Board& b) {
    TBBitboards bb = build_bitboards(b);
    unsigned castle = castling_mask(b);
    unsigned ep = ep_square(b);
    return ::tb_probe_dtz(bb.white, bb.black, bb.kings, bb.queens, bb.rooks,
                          bb.bishops, bb.knights, bb.pawns, b.halfMoveClock,
                          castle, ep, b.whiteToMove);
}

} // namespace nikola

