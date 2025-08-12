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

namespace nikola {

namespace {
// Helper to build bitboards from Board representation.
struct Bitboards {
    uint64_t white = 0;
    uint64_t black = 0;
    uint64_t kings = 0;
    uint64_t queens = 0;
    uint64_t rooks = 0;
    uint64_t bishops = 0;
    uint64_t knights = 0;
    uint64_t pawns = 0;
};

Bitboards build_bitboards(const Board& b) {
    Bitboards bb;
    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 8; ++c) {
            int8_t piece = b.squares[r][c];
            if (piece == Piece::EMPTY)
                continue;
            uint64_t sq = 1ULL << (r * 8 + c);
            if (piece > 0)
                bb.white |= sq;
            else
                bb.black |= sq;
            switch (piece) {
                case Piece::WP:
                case Piece::BP:
                    bb.pawns |= sq;
                    break;
                case Piece::WN:
                case Piece::BN:
                    bb.knights |= sq;
                    break;
                case Piece::WB:
                case Piece::BB:
                    bb.bishops |= sq;
                    break;
                case Piece::WR:
                case Piece::BR:
                    bb.rooks |= sq;
                    break;
                case Piece::WQ:
                case Piece::BQ:
                    bb.queens |= sq;
                    break;
                case Piece::WK:
                case Piece::BK:
                    bb.kings |= sq;
                    break;
                default:
                    break;
            }
        }
    }
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
    Bitboards bb = build_bitboards(b);
    unsigned castle = castling_mask(b);
    unsigned ep = ep_square(b);
    return ::tb_probe_wdl(bb.white, bb.black, bb.kings, bb.queens, bb.rooks,
                          bb.bishops, bb.knights, bb.pawns, b.halfMoveClock,
                          castle, ep, b.whiteToMove);
}

unsigned tbProbeRoot(const Board& b, unsigned* results) {
    Bitboards bb = build_bitboards(b);
    unsigned castle = castling_mask(b);
    unsigned ep = ep_square(b);
    return ::tb_probe_root(bb.white, bb.black, bb.kings, bb.queens, bb.rooks,
                           bb.bishops, bb.knights, bb.pawns, b.halfMoveClock,
                           castle, ep, b.whiteToMove, results);
}

} // namespace nikola

