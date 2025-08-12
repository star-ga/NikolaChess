#pragma once
#include <cstdint>

namespace nikola {

using Bitboard = uint64_t;

inline constexpr Bitboard bb_from_square(int sq) { return Bitboard{1} << sq; }
inline constexpr bool bb_is_set(Bitboard bb, int sq) { return (bb & bb_from_square(sq)) != 0; }
inline constexpr void bb_set(Bitboard &bb, int sq) { bb |= bb_from_square(sq); }
inline constexpr void bb_clear(Bitboard &bb, int sq) { bb &= ~bb_from_square(sq); }
inline int bb_popcount(Bitboard bb) { return __builtin_popcountll(bb); }
inline int bb_lsb(Bitboard bb) { return __builtin_ctzll(bb); }
inline Bitboard bb_pop_lsb(Bitboard &bb) {
    Bitboard l = bb & -bb;
    bb ^= l;
    return l;
}

struct Bitboards {
    Bitboard pieces[12]{}; // 0..5 white pawn..king, 6..11 black pawn..king
    Bitboard whiteMask{};
    Bitboard blackMask{};
    Bitboard occupied{};
};

Bitboards boardToBitboards(const struct Board &board);

} // namespace nikola

